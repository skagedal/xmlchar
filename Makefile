test_char: test_char.c bigtable.h pagedtable.h
	$(CC) -O3 -o $@ $< `pkg-config --cflags --libs glib-2.0`
	
bigtable.h: table_generator.py
	python $< --big $@
    
pagedtable.h: table_generator.py
	python $< --paged $@

