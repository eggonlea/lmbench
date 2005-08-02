all: index.html

index.html: Makefile
	echo "<html>" > index.html
	echo "<head><title>lmbench man pages</title></head>" >> index.html
	echo "<body>" >> index.html
	echo "<h1>lmbench man pages</h1>" >> index.html
	for s in 1 3 8; do \
		echo "<h2>Section $${s}</h2>" ;\
		echo "<ul>" ;\
		for f in `find ../../lmbench3/src ../../lmbench3/doc -name "*.$${s}"`; do \
			echo "<li> <a href=\"/cgi-bin/man?$${s}+`basename $$f .$${s}`\">"`basename $$f .$${s}`"</a>" ;\
		done; \
		echo "</ul>" ;\
	done >> index.html
	echo "<hr>" >> index.html
	echo "<table width=100%>" >> index.html
	echo "<tr>" >> index.html
	echo "<td align=left>" >> index.html
	echo "<address>Larry McVoy,<a href=mailto:lm@bitmover.com>lm@bitmover.com</a></address>" >> index.html
	echo "</tr>" >> index.html
	echo "</table>" >> index.html
	echo "<A href=\"http://sourceforge.net\"> <IMG src=\"http://sourceforge.net/sflogo.php?group_id=14418&amp;type=5\" width=\"210\" height=\"62\" border=\"0\" alt=\"SourceForge.net Logo\" /></A>" >> index.html
	echo "</body>" >> index.html
	echo "</html>" >> index.html

clean:
	rm -f *~ *.[1-8].html index.html
