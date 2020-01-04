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
ST_unsigned _levEditDistance(ST_unsigned len1, const lev_byte *string1, ST_unsigned len2, const lev_byte *string2,ST_int xcost);
ST_int _getStataRow(ST_int row);


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
	if(argc==3) return( _levEditDistance(strlen(argv[0]), argv[0], strlen(argv[1]), argv[1], atoi(argv[2])) );

	/////
	// Usage 2
	/////
	if(argc==1)
	{
		xcost = atoi(argv[0]);

		// Loop over rows and calculate edit distance for each pair of strings
		for(stata_row = SF_in1(); stata_row <= SF_in2(); stata_row++)
		{
			if(SF_ifobs(stata_row))
			{

				if(rc = SF_sdata(1,stata_row,str)) return(rc);
				if(rc = SF_sdata(2,stata_row,str2)) return(rc);
				if(rc = SF_vstore(3,stata_row, (double) _levEditDistance(strlen(str),str,strlen(str2),str2,xcost)  )) return(rc) ;
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
	The matches array is an NxN matrix of all the matches between those strings (1 signifies a match, 0 a non-match),
	where N is equal to the number of records. Its value is used to allocate memory to these arrays.
	*/
	char **string_data = (char**)malloc(num_records*sizeof(char*));
	ST_int **matches = (ST_int**)malloc(num_records*sizeof(ST_int*));

	// Read the string data and store it in our array. Account for possible specified "in" and "if"
	mat_row = 0;
	for(stata_row = SF_in1(); stata_row <= SF_in2(); stata_row++)
	{
		if(SF_ifobs(stata_row))
		{
			if(rc = SF_sdata(1,stata_row,str)) return(rc);

			// Add +1 for the null terminating character. Note: max string size is 244 in Stata
			string_data[mat_row] = (char*)malloc((strlen(str)+1)*sizeof(char));

			strcpy(string_data[mat_row],str);
			mat_row++;
		}
	}

	// Initialize our normalization factor
	norm_factor = 1;

	/*
	Calculate the Levenshtein distance between all combinations of strings. If the Levenshtein distance divided by
	the length of the smaller string is less than or equal to the threshold, we have a match.
	*/
	for(mat_row = 0; mat_row < num_records; mat_row++)
	{

		matches[mat_row] = (ST_int*)malloc(num_records*sizeof(ST_int)); // Allocate memory for one row of matches
		matches[mat_row][mat_row] = 1; // Strings obviously match with themselves. Get 1's down the diagonal.

		len1 = strlen(string_data[mat_row]);

		// (2,1) is the same as (1,2), so we only need to calculate one-half of the matrix.
		for(mat_col = mat_row+1; mat_col < num_records; mat_col++)
		{
			len2 = strlen(string_data[mat_col]);

			// Get max and min
			min_length = len1 <= len2 ? len1 : len2;
			max_length = len1 <= len2 ? len2 : len1;

			// Get our normalization factor (it has already been set equal to 1 at initialization)
			switch(normalize) {

				// Normalize by shorter string
				case 0:
					norm_factor = min_length;
					break;

				// Normalize by longer string
				case 1:
					norm_factor = max_length;
			}

			// Note: "marksample touse, strok" automatically omits strings of length 0 so we can assume norm_factor > 0

			// Skip pairs that cannot mathematically match
			if ( (max_length-min_length)/norm_factor > threshold ) {
				matches[mat_row][mat_col] = 0;
				continue;
			}

			// Determine matches based on the indexed Levenshtein distance and the threshold
			matches[mat_row][mat_col] = ( _levEditDistance(len1,string_data[mat_row],len2,string_data[mat_col],xcost) / norm_factor <= threshold);
		}
	}

	// Fill in the lower diagonal of the matrix using the values from the other half
	for(mat_row=1; mat_row < num_records; mat_row++)
	{
		for(mat_col = 0; mat_col < mat_row; mat_col++)
		{
			matches[mat_row][mat_col] = matches[mat_col][mat_row];
		}
	}


	/*
	We need to address situations where string A matches to String B and String C, but String B does not match String A.
	Essentially, we will broadly link as many strings as possible together. For example, if A=B, and B=C, then A=C.
	*/
	for(mat_col = 0; mat_col < num_records; mat_col++)
	{
		/*
		Keep looping over a particular string's matches until none if its matches
		match a string not already matched by this particular string. As we add matches to
		this string's list of matches, we want to remove them from the match matrix because
		we don't want a string to be in two different groups.

		Note: This code is not efficient, but it does not appear to be a bottleneck. Approximately 95% of
		the processing time goes towards calculating Levenshtein distances, so optimizing the post-processing
		sections of code is probably not worthwhile.
		*/

		do {
			restart = 0;
			for(mat_row = 0; mat_row < num_records; mat_row++)
			{
				if(mat_row==mat_col) continue; // Don't compare the string to itself

				// If one of column's matches matches another record...
				if(matches[mat_row][mat_col] == 1)
				{
					// Loop over that record's matches...
					for(k = 0; k < num_records; k++)
					{
						// If that record has a match...
						if(matches[k][mat_row]==1)
						{
							// That isn't already matched in record j...
							if(matches[k][mat_col] != 1)
							{
								// Add it to j's matches and set the do/while loop to be rerun
								matches[k][mat_col] = 1;
								restart = 1;
							}
							// Remove this match, since it now is already in string j's group
							matches[k][mat_row] = 0;
						}
					}
				}
			}

		} while(restart == 1);
	}



	/*
	Assign group numbers sequentially. We start with group #1, unless otherwise specified on the command line.
	*/
	group_num_increased = 0;
	for(mat_col = 0; mat_col < num_records; mat_col++)
	{
		group_num_increased = 0;
		for(mat_row = mat_col; mat_row < num_records; mat_row++)
		{
			if(matches[mat_row][mat_col] == 1)
			{
				if(group_num_increased==0)
				{
					group++;
					group_num_increased = 1;
				}

				// Store the group number in the Stata dataset. Variable 2 is our group variable
				if(rc = SF_vstore(2, _getStataRow(mat_row), (ST_double) group)) return(rc);
			}
		}
	}

	// Free up our memory and return
	for(k = 0; k < num_records; k++)
	{
		free(string_data[k]);
		free(matches[k]);
	}
	free(string_data);
	free(matches);
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
                 ST_int xcost)
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
  row = (ST_unsigned*)malloc(len2*sizeof(ST_unsigned));
  if (!row)
    return (ST_unsigned)(-1);
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
      }
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
      }
      /* lower triangle sentinel */
      if (i <= half) {
        ST_unsigned c3 = --D + (char1 != *char2p);
        x++;
        if (x > c3)
          x = c3;
        *p = x;
      }
    }
  }

  i = *end;
  free(row);
  return i;
}
