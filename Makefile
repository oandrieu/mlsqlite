include config.make

SRC_ML = sqlite3.ml sqlite3_big.ml sqlite3_str.ml
SRC_C  = ocaml-sqlite3.c ocaml-sqlite3-big.c

OBJ = $(SRC_ML:%.ml=%.cmo) $(SRC_ML:%.ml=%.cmx) $(SRC_C:%.c=%.o)

CPPFLAGS += $(SQLITE_CFLAGS)

lib : sqlite3.cma

sqlite3.cma : $(OBJ)
ifeq ($(STATIC), yes)
	$(OCAMLMKLIB) -v -custom -o sqlite3 -oc mlsqlite3 -cclib "$(SQLITE_LIBS)" $^
else
	$(OCAMLMKLIB) -v -o sqlite3 -oc mlsqlite3 $(SQLITE_LIBS) $^
endif

sqlite3.cmo : sqlite3.cmi
sqlite3.cmx : sqlite3.cmi
sqlite3_big.cmo : sqlite3_big.cmi
sqlite3_big.cmx : sqlite3_big.cmi
sqlite3_big.cmi : sqlite3.cmi
sqlite3_str.cmo : sqlite3_str.cmi
sqlite3_str.cmx : sqlite3_str.cmi
sqlite3_str.cmi : sqlite3.cmi

ocaml-sqlite3.o     : ocaml-sqlite3.h
ocaml-sqlite3-big.o : ocaml-sqlite3.h

%.cmo : %.ml
	$(OCAMLC) -c $<
%.cmx : %.ml
	$(OCAMLOPT) -c $<
%.cmi : %.mli
	$(OCAMLC) $<
%.o : %.c
	$(OCAMLC) -ccopt "$(CPPFLAGS)" $<

META : META.in
	sed 's/@VERSION@/$(VERSION)/' $< > $@

INSTALL_FILES = META sqlite3{,_big,_str}.{cmi,mli,cmx} sqlite3.{cma,cmxa,a} ocaml-sqlite3.h libmlsqlite3.a $(if $(STATIC),,dllmlsqlite3.so)
DIST_FILES    = README META META.in Makefile ocaml-sqlite3.h $(SRC_C) $(SRC_ML) $(SRC_ML:%.ml=%.mli) configure configure.ac acinclude.m4 aclocal.m4 config.h.in config.make.in doc

dist : ../$(TARNAME)-$(VERSION).tar.gz
../$(TARNAME)-$(VERSION).tar.gz : $(DIST_FILES)
	dir=$$(basename $$PWD) ; \
	cd .. ; \
	mv $$dir $(TARNAME)-$(VERSION) ; \
	tar zcvf $(TARNAME)-$(VERSION).tar.gz $(addprefix $(TARNAME)-$(VERSION)/,$(DIST_FILES)) ; \
	mv $(TARNAME)-$(VERSION) $$dir

doc : sqlite3.cmi sqlite3_big.cmi sqlite3_str.cmi
	mkdir -p doc
	ocamldoc -v -html -d doc -t "$(NAME) $(VERSION)" sqlite3.mli sqlite3_big.mli sqlite_str.mli

install : lib META
	$(OCAMLFIND) install $(TARNAME) $(INSTALL_FILES)

clean :
	rm -f *.cm* *.o *.a *.so META

configure: configure.ac acinclude.m4
	aclocal && autoconf
config.make: config.make.in config.status
	./config.status
config.status: configure
	./config.status --recheck

.PHONY : lib clean install dist doc
