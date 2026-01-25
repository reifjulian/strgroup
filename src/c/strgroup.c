/*
Group strings together according to how "similar they are.
Written by Julian Reif, 2007.05.02
Updated 2010.05.13
Updated 2010.07.28 - allocate memory equal exactly to size of string to decrease mem requirements.
*/

/*
Stata usage:

1)
plugin call strgroup string_var group_var [if] [in], threshold N group_num_start xcost normalize

2)
plugin call strgroup var1 var2 xcost

*/


// Libraries
#include "stplugin.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Definitions
typedef unsigned char lev_byte;

// Methods
ST_unsigned _levEditDistance(ST_unsigned len1, const lev_byte *string1, ST_unsigned len2, const lev_byte *string2, ST_int xcost, ST_unsigned *row_buffer, ST_unsigned max_dist);
ST_int _getStataRow(ST_int row);

// Global pointer for sort comparison (needed by qsort comparator)
static ST_int *g_string_lengths;

// Comparison function for sorting indices by string length
int compare_by_length(const void *a, const void *b) {
    ST_int idx_a = *(const ST_int*)a;
    ST_int idx_b = *(const ST_int*)b;
    return g_string_lengths[idx_a] - g_string_lengths[idx_b];
}

// Union-Find data structure for efficient connected component grouping
ST_int uf_find(ST_int *parent, ST_int i) {
    // Path compression: make every node point directly to the root
    if (parent[i] != i) {
        parent[i] = uf_find(parent, parent[i]);
    }
    return parent[i];
}

void uf_union(ST_int *parent, ST_int *rank, ST_int x, ST_int y) {
    ST_int root_x = uf_find(parent, x);
    ST_int root_y = uf_find(parent, y);

    if (root_x != root_y) {
        // Union by rank: attach smaller tree under larger tree
        if (rank[root_x] < rank[root_y]) {
            parent[root_x] = root_y;
        } else if (rank[root_x] > rank[root_y]) {
            parent[root_y] = root_x;
        } else {
            parent[root_y] = root_x;
            rank[root_x]++;
        }
    }
}


/*
This main calls the _levEditDistance() function and returns the output to Stata.

Usage 1:
argv[0]: String 1
argv[1]: String 2
argv[2]: Value for xcost (0/1)

Usage 2:
argv[0]: Value for xcost (0/1)

Usage 3:
argv[0]: Threshold value
argv[1]: Number of records being compared (N)
argv[2]: Where to start group numbers (default is 0)
argv[3]: Value for xcost (0/1)
argv[4]: Value for normalization. 0 = shorter string, 1 = longer string, 2 = none
*/

STDLL stata_call(ST_int argc, char *argv[] )
{

	// Declarations
	ST_int num_records;								// Number of strings being matched (N)
	char str[2050], str2[2050]; 					// Max string size in Stata is 2045 chars (add 1 for null \0 termination and then an extra to be safe)
	char msg[100];									// Holds any messages we want to transmit to Stata console
	ST_double threshold;   							// Threshold for determining whether or not two strings match
	ST_int len1, len2, min_length, max_length;  	// Length of two strings being compared
	ST_int mat_row, mat_col, k; 					// Matrix iterators
	ST_int restart;									// Used as a boolean
	ST_int group, group_num_increased; 		     	// Used for assigning group numbers
	ST_int stata_row;								// Stata row iterator
	ST_retcode rc;									// Stata return code
	ST_int xcost;									// Determines whether replace operation gets weight of 1 or 2
	ST_int normalize;								// Determines whether to match based on relative or absolute distances
	ST_double norm_factor;							// Normalization factor

	// # of args must be either 3, 4 or 5
	if (argc!=1 && argc!=3 && argc!=5) {
		int n;
		n=sprintf (msg, "Number of arguments must be 1, 3 or 5. You specified %d arguments\n", argc);
		SF_error(msg);
		return (-1);
	}


	///
	// Usage 1
	///
	if(argc==3) {
		ST_unsigned row_buffer[2050];  // Max string length in Stata is 2045
		return( _levEditDistance(strlen(argv[0]), argv[0], strlen(argv[1]), argv[1], atoi(argv[2]), row_buffer, (ST_unsigned)(-1)) );
	}

	/////
	// Usage 2
	/////
	if(argc==1)
	{
		xcost = atoi(argv[0]);
		ST_unsigned row_buffer[2050];  // Pre-allocated buffer for Levenshtein calculation

		// Loop over rows and calculate edit distance for each pair of strings
		for(stata_row = SF_in1(); stata_row <= SF_in2(); stata_row++)
		{
			if(SF_ifobs(stata_row))
			{

				if(rc = SF_sdata(1,stata_row,str)) return(rc);
				if(rc = SF_sdata(2,stata_row,str2)) return(rc);
				if(rc = SF_vstore(3,stata_row, (double) _levEditDistance(strlen(str),str,strlen(str2),str2,xcost,row_buffer,(ST_unsigned)(-1))  )) return(rc) ;
			}
		}
		return(0);
	}

	/////////////////////
	// Usage 3
	/////////////////////

	threshold = atof(argv[0]);
	num_records = atoi(argv[1]);
	group = atoi(argv[2]);
	xcost = atoi(argv[3]);
	normalize = atoi(argv[4]);


	/*
	The string_data array holds all the strings that are being compared.
	Union-Find is used for efficient grouping instead of an NxN matches matrix.
	This reduces memory from O(N^2) to O(N) and eliminates the transitive closure phase.
	*/
	char **string_data = (char**)malloc(num_records*sizeof(char*));
	ST_int *string_lengths = (ST_int*)malloc(num_records*sizeof(ST_int));  // Pre-computed string lengths
	ST_int *stata_rows = (ST_int*)malloc(num_records*sizeof(ST_int));  // Pre-computed Stata row mapping
	ST_int *sorted_idx = (ST_int*)malloc(num_records*sizeof(ST_int));  // Indices sorted by string length
	ST_int *uf_parent = (ST_int*)malloc(num_records*sizeof(ST_int));  // Union-Find parent array
	ST_int *uf_rank = (ST_int*)malloc(num_records*sizeof(ST_int));  // Union-Find rank array for union by rank
	ST_unsigned *lev_row_buffer = (ST_unsigned*)malloc(2050*sizeof(ST_unsigned));  // Pre-allocated buffer for Levenshtein

	// Initialize Union-Find and sorted index array
	for(k = 0; k < num_records; k++) {
		uf_parent[k] = k;
		uf_rank[k] = 0;
		sorted_idx[k] = k;
	}

	// Read the string data and store it in our array. Account for possible specified "in" and "if"
	mat_row = 0;
	for(stata_row = SF_in1(); stata_row <= SF_in2(); stata_row++)
	{
		if(SF_ifobs(stata_row))
		{
			if(rc = SF_sdata(1,stata_row,str)) return(rc);

			// Add +1 for the null terminating character. Note: max string size is 244 in Stata
			ST_int slen = strlen(str);
			string_data[mat_row] = (char*)malloc((slen+1)*sizeof(char));
			string_lengths[mat_row] = slen;  // Store pre-computed length
			stata_rows[mat_row] = stata_row;  // Store Stata row mapping

			strcpy(string_data[mat_row],str);
			mat_row++;
		}
	}

	// Sort indices by string length for early termination optimization
	g_string_lengths = string_lengths;
	qsort(sorted_idx, num_records, sizeof(ST_int), compare_by_length);

	// Initialize our normalization factor
	norm_factor = 1;

	/*
	Calculate the Levenshtein distance between all combinations of strings. If the Levenshtein distance divided by
	the length of the smaller string is less than or equal to the threshold, we have a match.
	Union-Find handles transitive closure automatically - no separate phase needed.
	Strings are processed in sorted order by length to enable early termination.
	*/
	ST_int i, j, idx_i, idx_j;
	for(i = 0; i < num_records; i++)
	{
		idx_i = sorted_idx[i];
		len1 = string_lengths[idx_i];  // Use pre-computed length (shorter or equal)

		// Compare with strings of equal or greater length (in sorted order)
		for(j = i+1; j < num_records; j++)
		{
			idx_j = sorted_idx[j];
			len2 = string_lengths[idx_j];  // Use pre-computed length (longer or equal since sorted)

			// Since sorted by length: len1 <= len2, so min_length = len1, max_length = len2
			min_length = len1;
			max_length = len2;

			// Get our normalization factor
			switch(normalize) {
				case 0:  // Normalize by shorter string
					norm_factor = min_length;
					break;
				case 1:  // Normalize by longer string
					norm_factor = max_length;
					break;
				// case 2: norm_factor stays at 1 (no normalization)
			}

			// Note: "marksample touse, strok" automatically omits strings of length 0 so we can assume norm_factor > 0

			// Early termination: if length difference alone exceeds threshold, all remaining strings will too
			// (since they're sorted by length and only get longer)
			if ( (max_length-min_length)/norm_factor > threshold ) {
				if (normalize == 0) {
					// Normalizing by shorter string: once len2 is too big, all subsequent are too big
					break;
				}
				// For normalize=1 or 2, we can't break early, just skip this pair
				continue;
			}

			// Determine matches based on the indexed Levenshtein distance and the threshold
			ST_unsigned max_dist = (ST_unsigned)(threshold * norm_factor);
			if ( _levEditDistance(len1,string_data[idx_i],len2,string_data[idx_j],xcost,lev_row_buffer,max_dist) / norm_factor <= threshold) {
				// Strings match - union them in the Union-Find structure
				uf_union(uf_parent, uf_rank, idx_i, idx_j);
			}
		}
	}



	/*
	Assign group numbers sequentially using Union-Find roots.
	Each unique root gets a new group number. We use a mapping array to track which roots
	have been assigned group numbers.
	*/
	ST_int *root_to_group = (ST_int*)malloc(num_records*sizeof(ST_int));
	for(k = 0; k < num_records; k++) {
		root_to_group[k] = -1;  // -1 means not yet assigned
	}

	for(mat_row = 0; mat_row < num_records; mat_row++)
	{
		ST_int root = uf_find(uf_parent, mat_row);

		// If this root hasn't been assigned a group yet, assign one
		if(root_to_group[root] == -1) {
			group++;
			root_to_group[root] = group;
		}

		// Store the group number in the Stata dataset. Variable 2 is our group variable
		if(rc = SF_vstore(2, stata_rows[mat_row], (ST_double) root_to_group[root])) return(rc);
	}

	// Free up our memory and return
	for(k = 0; k < num_records; k++)
	{
		free(string_data[k]);
	}
	free(string_data);
	free(string_lengths);
	free(stata_rows);
	free(sorted_idx);
	free(uf_parent);
	free(uf_rank);
	free(root_to_group);
	free(lev_row_buffer);
	return(0);
}


/*
This method takes the matrix row and returns the corresponding number for the Stata datset. These are
the same if no "if" or "in" statement was specified in Stata.
*/
ST_int _getStataRow(ST_int row)
{
	ST_int stata_row = SF_in1() - 1; //Set stata_row to the beginning of our "in" range

	ST_int i;
	for(i = 0; i <= row; i++)
	{
		stata_row++;
		while(!SF_ifobs(stata_row))
		{
			stata_row++; //Skip excluded "if" observations
		}
	}

	return stata_row;
}



/*
 * _levEditDistance:
 * @len1: The length of @string1.
 * @string1: A sequence of bytes of length @len1, may contain NUL characters.
 * @len2: The length of @string2.
 * @string2: A sequence of bytes of length @len2, may contain NUL characters.
 * @xcost: If nonzero, the replace operation has weight 2, otherwise all
 *         edit operations have equal weights of 1.
 *
 * Computes Levenshtein edit distance of two strings.
 *
 * Returns: The edit distance.


lev_edit_distance() was adapted from a Python module:
Copyright (C) 2002-2003 David Necas (Yeti) <yeti@physics.muni.cz>.
The original code can be obtained at:
http://code.google.com/p/pylevenshtein/source/browse/trunk/Levenshtein.c


The levenshtein() function returns the Levenshtein distance between two strings.

The Levenshtein distance is the number of characters you have to replace, insert or delete to transform string1 into string2.

By default, PHP gives each operation (replace, insert, and delete) equal weight. However, you can define the cost of each operation by setting the optional insert, replace, and delete parameters.
*/
ST_unsigned _levEditDistance(ST_unsigned len1, const lev_byte *string1,
                  ST_unsigned len2, const lev_byte *string2,
                 ST_int xcost, ST_unsigned *row_buffer, ST_unsigned max_dist)
{
  ST_unsigned i;
  ST_unsigned *row;  /* we only need to keep one row of costs */
  ST_unsigned *end;
  ST_unsigned half;

  /* strip common prefix */
  while (len1 > 0 && len2 > 0 && *string1 == *string2) {
    len1--;
    len2--;
    string1++;
    string2++;
  }

  /* strip common suffix */
  while (len1 > 0 && len2 > 0 && string1[len1-1] == string2[len2-1]) {
    len1--;
    len2--;
  }

  /* catch trivial cases */
  if (len1 == 0)
    return len2;
  if (len2 == 0)
    return len1;

  /* make the inner cycle (i.e. string2) the longer one */
  if (len1 > len2) {
    ST_unsigned nx = len1;
    const lev_byte *sx = string1;
    len1 = len2;
    len2 = nx;
    string1 = string2;
    string2 = sx;
  }
  /* check len1 == 1 separately */
  if (len1 == 1) {
    if (xcost)
      return len2 + 1 - 2*(memchr(string2, *string1, len2) != NULL);
    else
      return len2 - (memchr(string2, *string1, len2) != NULL);
  }
  len1++;
  len2++;
  half = len1 >> 1;

  /* initalize first row */
  row = row_buffer;
  end = row + len2 - 1;
  for (i = 0; i < len2 - (xcost ? 0 : half); i++)
    row[i] = i;

  /* go through the matrix and compute the costs.  yes, this is an extremely
   * obfuscated version, but also extremely memory-conservative and relatively
   * fast.  */
  if (xcost) {
    for (i = 1; i < len1; i++) {
      ST_unsigned *p = row + 1;
      const lev_byte char1 = string1[i - 1];
      const lev_byte *char2p = string2;
      ST_unsigned D = i;
      ST_unsigned x = i;
      ST_unsigned row_min = (ST_unsigned)(-1);  // Track minimum value in this row
      while (p <= end) {
        if (char1 == *(char2p++))
          x = --D;
        else
          x++;
        D = *p;
        D++;
        if (x > D)
          x = D;
        *(p++) = x;
        if (x < row_min) row_min = x;  // Update minimum
      }
      // Early termination: if minimum in this row exceeds max_dist, no point continuing
      if (max_dist != (ST_unsigned)(-1) && row_min > max_dist)
        return row_min;
    }
  }
  else {
    /* in this case we don't have to scan two corner triangles (of size len1/2)
     * in the matrix because no best path can go throught them. note this
     * breaks when len1 == len2 == 2 so the memchr() special case above is
     * necessary */
    row[0] = len1 - half - 1;
    for (i = 1; i < len1; i++) {
      ST_unsigned *p;
      const lev_byte char1 = string1[i - 1];
      const lev_byte *char2p;
      ST_unsigned D, x;
      ST_unsigned row_min = (ST_unsigned)(-1);  // Track minimum value in this row
      /* skip the upper triangle */
      if (i >= len1 - half) {
        ST_unsigned offset = i - (len1 - half);
        ST_unsigned c3;

        char2p = string2 + offset;
        p = row + offset;
        c3 = *(p++) + (char1 != *(char2p++));
        x = *p;
        x++;
        D = x;
        if (x > c3)
          x = c3;
        *(p++) = x;
        if (x < row_min) row_min = x;
      }
      else {
        p = row + 1;
        char2p = string2;
        D = x = i;
      }
      /* skip the lower triangle */
      if (i <= half + 1)
        end = row + len2 + i - half - 2;
      /* main */
      while (p <= end) {
        ST_unsigned c3 = --D + (char1 != *(char2p++));
        x++;
        if (x > c3)
          x = c3;
        D = *p;
        D++;
        if (x > D)
          x = D;
        *(p++) = x;
        if (x < row_min) row_min = x;
      }
      /* lower triangle sentinel */
      if (i <= half) {
        ST_unsigned c3 = --D + (char1 != *char2p);
        x++;
        if (x > c3)
          x = c3;
        *p = x;
        if (x < row_min) row_min = x;
      }
      // Early termination: if minimum in this row exceeds max_dist, no point continuing
      if (max_dist != (ST_unsigned)(-1) && row_min > max_dist)
        return row_min;
    }
  }

  i = *end;
  return i;
}
