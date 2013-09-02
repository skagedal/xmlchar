xmlchar
=======

Some code to generate quick lookup for XML character classes.

I wanted some code to quickly check if a character belongs to a class of 
Unicode characters defined by ranges, specifically the productions 
NameStartChar and NameChar in the XML 1.0 Recommendation, 5th ed.

Here are three variants:

- naive: simple branching range checks
- bigtable: using a huge lookup table, one bit for every code point in Unicode.
- pagedtable: using a paged lookup table.

The output of the test_char program displays the following results on my
computer (with an Intel Core 2 Duo Processor T5200, 1.60 GHz):

NameChar
--------
- naive:
  - takes 2.7125 seconds to run through all unicode 100 times
  - takes 0.9091 seconds to run through ASCII 800000 times
- bigtable:
  - uses 136 kilobytes.
  - works.
  - takes 0.4214 seconds to run through all unicode 100 times
  - takes 0.4003 seconds to run through ASCII 800000 times
- pagedtable:
  - uses 2 kilobytes.
  - works.
  - takes 0.6848 seconds to run through all unicode 100 times
  - takes 0.6561 seconds to run through ASCII 800000 times

NameStartChar
-------------
- naive:
  - takes 2.2110 seconds to run through all unicode 100 times
  - takes 0.7148 seconds to run through ASCII 800000 times
- bigtable:
  - uses 136 kilobytes.
  - works.
  - takes 0.4208 seconds to run through all unicode 100 times
  - takes 0.3973 seconds to run through ASCII 800000 times
- pagedtable:
  - uses 2 kilobytes.
  - works.
  - takes 0.6913 seconds to run through all unicode 100 times
  - takes 0.6585 seconds to run through ASCII 800000 times

So, the benefit of the paged lookup table is not huge, especially not when the 
checked characters are in the ASCII range, which probably most XML tag names 
will be.  The bigtable is slightly better, but not really worth the space?

Well, it was fun to code anyway...


