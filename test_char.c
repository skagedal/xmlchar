#include <stdio.h>
#include <time.h>
#include <glib.h>

#include "bigtable.h"
#include "pagedtable.h"

#define TIMES 100

gboolean naive_is_name_start_char (gunichar c)
{
  if (c == ':' || 
      (c >= 'A' && c <= 'Z') ||
      c == '_' ||
      (c >= 'a' && c <= 'z'))
    return TRUE;
  if (c <  0xC0) return FALSE;
  if (c <= 0xD6) return TRUE;
  if (c <  0xD8) return FALSE;
  if (c <= 0xF6) return TRUE;
  if (c <  0xF8) return FALSE;
  if (c <= 0x2FF) return TRUE;
  if (c <  0x370) return FALSE;
  if (c <= 0x37D) return TRUE;
  if (c <  0x37F) return FALSE;
  if (c <= 0x1FFF) return TRUE;
  if (c <  0x200C) return FALSE;
  if (c <= 0x200D) return TRUE;
  if (c <  0x2070) return FALSE;
  if (c <= 0x218F) return TRUE;
  if (c <  0x2C00) return FALSE;
  if (c <= 0x2FEF) return TRUE;
  if (c <  0x3001) return FALSE;
  if (c <= 0xD7FF) return TRUE;
  if (c <  0xF900) return FALSE;
  if (c <= 0xFDCF) return TRUE;
  if (c <  0xFDF0) return FALSE;
  if (c <= 0xFFFD) return TRUE;
  if (c <  0x10000) return FALSE;
  if (c <= 0xEFFFF) return TRUE;
  return FALSE;
}

gboolean naive_is_name_char (gunichar c)
{
  if (naive_is_name_start_char (c))
    return TRUE;
  if (c == '-' || c == '.' ||
      (c >= '0' && c <= '9') ||
      c == 0xB7)
    return TRUE;
  if (c <  0x0300) return FALSE;
  if (c <= 0x036F) return TRUE;
  if (c <  0x203F) return FALSE;
  if (c <= 0x2040) return TRUE;
  return FALSE;
}

gboolean
bigtable_is_name_char (gunichar c)
{
  return ((NameChar[c / 32] & (1 << (c % 32))) != 0);
}

gboolean
bigtable_is_name_start_char (gunichar c)
{
  return ((NameStartChar[c / 32] & (1 << (c % 32))) != 0);
}

/* We have less than 16 pages, so 4 bits is used to index pages. */
#define PAGE_INDEX_SIZE 4 
/* Number of bits used in each page */
#define PAGE_SIZE 10

gboolean
pagedtable_is_name_char (gunichar c)
{
  const guint32 *page;
/*
 * c is an unicode code point, probably originating from UTF-8, validated
 * and transformed according to RFC 3629.  It is ensured
 * to be no larger than 0x10FFFF. So it has 21 significant bits.
 * We divide this into page_index and bit_index at PAGE_SIZE.  
 * Here, PAGE_SIZE is 10.
 *
 *  20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *  \______________________________/ \___________________________/
 *                   |                                |
 *               page_index                       bit_index
 *
 * page_index is used to index that data in the NameChar array,
 * which is an array of 32 bit ints.  In each int we can pack
 * eight 4-bit indices that point at the actual page number, from
 * 0 to 15.  Three bits needed to index eight parts of the 32 bit word.
 * So we can break it down further as:
 *
 *  20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *  \_____________________/ \______/
 *            |                |
 *       page_index_a      page_index_b
 * 
 * In the end, we just want one of the bits that are packed into 
 * 32 bit ints.  We divide bit_index similarly:
 *
 *  20 19 18 17 16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 *                                   \____________/ \____________/
 *                                           |             |
 *                                     bit_index_a     bit_index_b
 *
 * Thus, to unpack all bits:
 */
  gint page_index_a, page_index_b, bit_index_a, bit_index_b;
  gint page_number;

  bit_index_b = c & 0x1F;      /* 2^5 - 1 */
  c >>= 5;
  bit_index_a = c & 0x1F;      /* 2^5 - 1 */
  c >>= 5;
  page_index_b = c & 0x07;     /* 2^3 - 1 */
  c >>= 3;
  page_index_a = c;
  
  /* Then we can find the page: */
  
  page_number = (NameChar_index [page_index_a] >> (page_index_b << 2)) & 0xF;
  
  page = NameChar_pages [page_number];
  
  return (page[bit_index_a] >> bit_index_b) & 0x1;
}

gboolean
pagedtable_is_name_start_char (gunichar c)
{
  const guint32 *page;
  gint page_index_a, page_index_b, bit_index_a, bit_index_b;
  gint page_number;

  bit_index_b = c & 0x1F;      /* 2^5 - 1 */
  c >>= 5;
  bit_index_a = c & 0x1F;      /* 2^5 - 1 */
  c >>= 5;
  page_index_b = c & 0x07;     /* 2^3 - 1 */
  c >>= 3;
  page_index_a = c;
  
  page_number = (NameStartChar_index [page_index_a] >> (page_index_b << 2)) & 0xF;
  page = NameStartChar_pages [page_number];
  
  return (page[bit_index_a] >> bit_index_b) & 0x1;
}

typedef gboolean (*chartest_func) (gunichar c);

gboolean
test_valid_name_char_func (gchar *s, chartest_func f)
{
  int i;
  for (i = 0; i < 0x110000; i++)
    {
      if (f(i) != naive_is_name_char(i))
	{
	  gchar buf[8];
	  buf [g_unichar_to_utf8 (i, buf)] = '\0';
	  g_critical ("%s returns %d on char 0x%x (%s), where naive implementation returns %d",
		      s, f(i), i, buf, naive_is_name_char(i));
	  return FALSE;
	}
    }
  return TRUE;
}

gboolean
test_valid_name_start_char_func (gchar *s, chartest_func f)
{
  int i;
  for (i = 0; i < 0x110000; i++)
    {
      if (f(i) != naive_is_name_start_char(i))
	{
	  gchar buf[8];
	  buf [g_unichar_to_utf8 (i, buf)] = '\0';
	  g_critical ("%s returns %d on char 0x%x (%s), where naive implementation returns %d",
		      s, f(i), i, buf, naive_is_name_start_char(i));
	  return FALSE;
	}
    }
  return TRUE;
}

gdouble 
timespec_to_double (struct timespec *t)
{
  return t->tv_sec + (gdouble)t->tv_nsec / 1000000000.0;
}

gdouble
time_diff (struct timespec *b, struct timespec *a)
{
  return timespec_to_double (b) - timespec_to_double (a);
}

gdouble
test_time (chartest_func f)
{
  struct timespec start, end;
  int i, j;
  g_assert (clock_gettime (CLOCK_MONOTONIC, &start) == 0);
  for (i = 0; i < TIMES; i++) 
    {
      for (j = 0; j < 0x110000; j++)
	f (j);
    }
  g_assert (clock_gettime (CLOCK_MONOTONIC, &end) == 0);
  return time_diff (&end, &start);
}

gdouble
test_time_ascii (chartest_func f)
{
  struct timespec start, end;
  int i, j;
  g_assert (clock_gettime (CLOCK_MONOTONIC, &start) == 0);
  for (i = 0; i < TIMES * 8000; i++) 
    {
      for (j = 0; j < 128; j++)
	f (j);
    }
  g_assert (clock_gettime (CLOCK_MONOTONIC, &end) == 0);
  return time_diff (&end, &start);
}

void
print_time (chartest_func f)
{
  printf ("  - takes %.4f seconds to run through all unicode %d times\n", 
	  test_time (f), TIMES);
  printf ("  - takes %.4f seconds to run through ASCII %d times\n",
	  test_time_ascii (f), TIMES * 8000);
}

int
main (int argc, char *argv[])
{
  printf ("== NameChar == \n");
  printf ("- naive:\n");
  print_time (naive_is_name_char);

  printf ("- bigtable:\n  - uses %d kilobytes.\n", sizeof NameChar / 1024);
  if (test_valid_name_char_func ("bigtable_is_name_char", 
				 bigtable_is_name_char))
    printf ("  - works.\n");
  print_time (bigtable_is_name_char);

  printf ("- pagedtable:\n  - uses %d kilobytes.\n", 
  	  (sizeof NameChar_index + sizeof NameChar_pages) / 1024);
  if (test_valid_name_char_func ("pagedtable_is_name_char", 
				 pagedtable_is_name_char))
    printf ("  - works.\n");
  print_time (pagedtable_is_name_char);

  printf ("\n== NameStartChar ==\n");
  printf ("- naive:\n");
  print_time (naive_is_name_start_char);

  printf ("- bigtable:\n  - uses %d kilobytes.\n", sizeof NameStartChar / 1024);
  if (test_valid_name_start_char_func ("bigtable_is_name_start_char", 
				 bigtable_is_name_start_char))
    printf ("  - works.\n");
  print_time (bigtable_is_name_start_char);

  printf ("- pagedtable:\n  - uses %d kilobytes.\n", 
  	  (sizeof NameStartChar_index + sizeof NameStartChar_pages) / 1024);
  if (test_valid_name_start_char_func ("pagedtable_is_name_start_char", 
				 pagedtable_is_name_start_char))
    printf ("  - works.\n");
  print_time (pagedtable_is_name_start_char);

}

