# -*- python-indent: 4 -*-

from __future__ import unicode_literals, division, print_function

import sys

# Numbers in [brackets] refers to productions in XML 1.0, 5th ed.

# [4]
NameStartChar = """ 
        ":" | [A-Z] | "_" | [a-z] | [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] 
        | [#x370-#x37D] | [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] 
        | [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] 
        | [#x10000-#xEFFFF] 
        """

# [4a]
NameChar = NameStartChar + """
        | "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]
        """

# A Pointset is a set of code points, i.e., a class that represents
# a production like one of the above.

class Pointset:
    def __init__(self, production):
        """Creates a Pointset by parsing a grammar production."""
        def parse_char(c):
            if (c.startswith("#x")):
                return int(c[2:], 16)
            else:
                return ord(c)

        def parse_range (s):
            s = s.strip()
            if s[0] == '"' and s[-1] == '"':
                val = parse_char (s[1:-1])
                return (val, val) 
            elif s[0] == '[' and s[-1] == ']':
                return tuple(parse_char(c) for c in s[1:-1].split("-"))
            else:
                val = parse_char (s)
                return (val, val)
        self.ranges = sorted ([parse_range(s) for s in production.split("|")])

    def contains (self, x):
        """Checks if the Pointset includes the code point x."""
        for (a, b) in self.ranges:
            if x < a:
                return False
            if x <= b:
                return True
        return False

# Here are functions to create the (paged) bitmaps.

def make_bitmap (pointset, less_than):
    ints_to_make = less_than // 32
    for i in range(ints_to_make):
        bmp = 0
        for j in range(32):
            if pointset.contains (i * 32 + j):
                bmp |= 1 << j
        yield bmp


def make_page (pointset, page_size, page_num):
    values_per_page = 1 << page_size
    words_per_page = values_per_page >> 5
    page = []
    for i in xrange(words_per_page):
        word = 0
        for j in xrange(32):
            codepoint = page_num * values_per_page + i * 32 + j
            if pointset.contains(codepoint):
                word |= 1 << j
        page.append (word)
    return page

def make_pages (pointset, page_size):
    """page_size in bits"""
    num_page_pointers = 1 << (21 - page_size)
    pages = []
    page_pointers = []
    for i in xrange(num_page_pointers):
        page = make_page (pointset, page_size, i)
        if not page in pages:
            pages.append (page)
        page_pointers.append (pages.index (page))
    
    return pages, page_pointers

def bits_per_page_pointer (npages):
    bitsperpagepointer = 1
    while 1 << bitsperpagepointer < npages:
        bitsperpagepointer <<= 1
    return bitsperpagepointer


def make_words (pointers, bits_per_pointer):
    word = 0
    filled_bits = 0
    for pointer in pointers:
        word |= (pointer << filled_bits)
        filled_bits += bits_per_pointer
        if filled_bits == 32:
            yield word
            filled_bits = 0
            word = 0
        assert (filled_bits < 32)

# Functions to write C code

def write_array (f, name, words):
    f.write("static const guint32 %s[] =\n  {" % name)

    for i, word in enumerate(words):
        if i > 0:
            f.write(",")
        if i % 4 == 0:
            f.write("\n    ")
        else:
            f.write(" ")
        f.write("0x{0:08x}".format(word))

    f.write("\n  };\n\n")

def write_2dim_array (f, name, wwords):
    n = len (wwords [0])
    f.write ("static const guint32 %s[][%d] =\n  {\n" % (name, n))
    isfirst = True
    for words in wwords:
        if not isfirst:
            f.write (",\n")
        isfirst = False
        f.write("    {")
        for i, word in enumerate(words):
            if i > 0:
                f.write(",")
            if i % 4 == 0:
                f.write("\n      ")
            else:
                f.write(" ")
            f.write("0x{0:08x}".format(word))
        f.write("\n    }")

    f.write("\n  };\n\n")

def write_paged_bitmap (f, name, pointset, psize):
    pages, page_pointers = make_pages (pointset, psize)
    indexname = "%s_index" % name
    pagesname = "%s_pages" % name
    write_array (f, indexname, make_words (page_pointers, 
                                        bits_per_page_pointer (len (pages))))
    write_2dim_array (f, pagesname, pages)

def write_big_bitmap (f, name, pointset):
    i = 0
    bmp_size = 0x110000
    f.write("static const guint32 %s[] =\n  {\n" % name)
    for bmp in make_bitmap (pointset, bmp_size):
        if i % 4 == 0:
            f.write("    ")
            
        f.write("0x{0:08x}".format(bmp))
        if i < (bmp_size // 32) - 1:
            f.write(", ")
        if i % 4 == 3:
            f.write("\n")
        i += 1

    if i % 4 != 0:
        f.write("\n")
    f.write("  };\n\n")

# Analyze bitmaps

def find_best_page_size(pointset):
    """This is be used to find the best page size.  For NameChar,
    it seems a page size of 10 bits is most space efficient."""
    for psize in [8, 9, 10, 11, 12, 13, 14]:
        pages, page_pointers = make_pages (pointset, psize)
        print("Page size: %d" % psize)
        npages = len(pages)
        print("Number of pages: %d" % npages)
        npagepointers = len(page_pointers)
        print("Number of page pointers: %d" % npagepointers)
        bitsperpagepointer = bits_per_page_pointer (npages)
        print("Bits per page pointer: %d" % bitsperpagepointer)
        pointersperword = 32 / bitsperpagepointer
        s1 = (npagepointers / pointersperword) * 4 
        print("Size for page index: %d B" % s1)
        sp = (1 << psize) / 8
        print("Size per page: %d B" % sp)
        s2 = sp * npages
        print("Size to store all pages: %d B" % s2)
        stot = s1 + s2
        print("Total size: %d B (%d kB)" % (stot, stot / 1024))
        print("-----")

def usage_exit ():
    print ("usage: %s --help | --test | --big <outfile> | --paged <outfile>" % sys.argv[0])
    sys.exit(0)

if __name__ == "__main__":
    name_char = Pointset (NameChar)
    name_start_char = Pointset (NameStartChar)

    if len (sys.argv) < 2:
        usage_exit ()
        
    if len (sys.argv) < 3:
        if sys.argv[1] == "--test":
            print("== NameStartChar ==")
            find_best_page_size (name_start_char)
            print("== NameChar ==")
            find_best_page_size (name_char)
            sys.exit (0)
        else:
            usage_exit ()
            
    if sys.argv[1] == "--big":
        f = open (sys.argv[2], "w")
        write_big_bitmap (f, "NameStartChar", name_start_char)
        write_big_bitmap (f, "NameChar", name_char)
    elif sys.argv[1] == "--paged":
        f = open (sys.argv[2], "w")
        write_paged_bitmap (f, "NameChar", name_char, 10)
        write_paged_bitmap (f, "NameStartChar", name_start_char, 10)

