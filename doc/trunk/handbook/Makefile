#Makefile for m0n0doc; produces HTML and PDF files from XML files

#As currently set up, producing HTML files requires the xsltproc program
#and the docbook xsl files; producing a PDF file requires Java and some
#Java packages (defined later in the Makefile).

#Tools
DOCBOOK_DIR   = /usr/local/share/xsl/docbook
HTML_DIR_XSL  = $(DOCBOOK_DIR)/html/chunk.xsl
HTML_FILE_XSL = $(DOCBOOK_DIR)/html/docbook.xsl
FO_FILE_XSL   = $(DOCBOOK_DIR)/fo/docbook.xsl
XML_FILES     = $(wildcard *.xml)
MAKEFILE      = Makefile

#Output files
BASENAME      = m0n0doc
HTML_DIR      = html.d
HTML_FILE     = $(BASENAME).html
FO_FILE       = $(BASENAME).fo
PDF_FILE      = $(BASENAME).pdf

#Java tools for making pdf file, see comments below
JAVA = /usr/local/jdk/bin/java
FOP = /u/net/fop-0.20.5/fop.sh
SAXONLIB = /u/net/saxon6_5_2/saxon.jar

#If you need to customize any of the above defines, edit your own localdefs.mak
-include localdefs.mak

default:	html_dir

all:		html_dir html_file pdf_file

#Produce a directory with a set of HTML files, one per chapter or section
html_dir:	$(HTML_DIR)/index.html

$(HTML_DIR)/index.html:	$(XML_FILES) $(MAKEFILE)
	@echo Creating html directory
	[ -d $(HTML_DIR) ] || mkdir $(HTML_DIR)
	xsltproc \
		--stringparam section.autolabel 1 \
		--stringparam section.label.includes.component.label 1 \
		--stringparam toc.max.depth 2 \
		--stringparam html.stylesheet m0n0doc.css \
		--stringparam chunker.output.indent yes \
		--stringparam base.dir $(HTML_DIR)/ \
		$(HTML_DIR_XSL) \
		book.xml
	cp -p m0n0doc.css $(HTML_DIR)
	tar cf - screens/*/*.png | ( cd $(HTML_DIR) && tar xf -)
	cp -p *.png $(HTML_DIR)

#Produce a single HTML file with everything in it
html_file:	$(XML_FILES) $(MAKEFILE)
	@echo Creating html file
	xsltproc \
		--stringparam section.autolabel 1 \
		--stringparam section.label.includes.component.label 1 \
		--stringparam toc.max.depth 2 \
		--stringparam html.stylesheet m0n0doc.css \
		-o $(HTML_FILE) \
		$(HTML_FILE_XSL) \
		book.xml


#Is there an open source implementation of an FO/PDF converter other than
#a Java implementation?  If someone knows of a good one, this section can
#be modified to use that rather than Java; meanwhile, if you want to produce
#PDF files from the XML files, you'll have to download Java, fop, and saxon.
SAXON = $(JAVA) -jar $(SAXONLIB)
#In order to convert the PNG files and get them included in the PDF output
#file, you need to set up either JIMI or JAI.  For details, see
# http://xml.apache.org/fop/graphics.html#jai

#Produce a PDF file.  This implementation requires Java.
pdf_file:	fo_file
	@echo Creating pdf file
	JAVACMD=$(JAVA) $(FOP) -fo $(FO_FILE) -pdf $(PDF_FILE)

#The FO (format object) file is an intermediate file used to create the PDF file
fo_file:	$(XML_FILES) $(MAKEFILE)
	$(SAXON) -o $(FO_FILE) \
		book.xml $(FO_FILE_XSL) \
		section.autolabel=1 \
		section.label.includes.component.label=1 \
		toc.max.depth=2 \
		html.stylesheet=m0n0doc.css \
		fop.extensions=1 \
		insert.xref.page.number=1

#Remove all generated files
clean:	clean_html_dir clean_html_file clean_pdf_file

#Remove the html directory
clean_html_dir:;	rm -rf $(HTML_DIR)

#Remove the all-in-one html output file
clean_html_file:;	rm -f $(HTML_FILE)

#Remove the PDF file and intermediate files used to create it
clean_pdf_file:;	rm -f $(PDF_FILE) $(FO_FILE)
