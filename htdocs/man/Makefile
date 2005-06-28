all: clean
	for f in ../../lmbench3/doc/*.[1-8]; do \
		rman -f HTML -S -r "%s.%s.html" $$f | \
		sed -e 's%</body>%<A href=\"http://sourceforge.net\"> <IMG src=\"http://sourceforge.net/sflogo.php?group_id=14418&amp;type=5\" width=\"210\" height=\"62\" border=\"0\" alt=\"SourceForge.net Logo\" /></A></body>%' > `basename $$f`.html; \
	done
	make index.html

index.html:
	echo "<html>" > index.html
	echo "<head><title>lmbench man pages</title></head>" >> index.html
	echo "<body>" >> index.html
	echo "<h1>lmbench man pages</h1>" >> index.html
	for s in 1 3 8; do \
		echo "<h2>Section $${s}</h2>" >> index.html; \
		echo "<ul>" >> index.html; \
		for f in *.$${s}.html; do \
			echo "<li> <a href=\"$$f\">"`basename $$f .$${s}.html`"</a>" >> index.html; \
		done; \
		echo "</ul>" >> index.html; \
	done
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
