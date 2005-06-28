all: clean
	for f in ../../lmbench3/doc/*.[1-8]; do \
		rman -f HTML -S -r "%s.%s.html" $$f > `basename $$f`.html; \
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
			echo "<li> <a href=\"$$f\">"`basename $$f $${s}.html`"</a>" >> index.html; \
		done; \
		echo "</ul>" >> index.html; \
	done
	echo "</body>" >> index.html
	echo "</html>" >> index.html

clean:
	rm -f *~ *.[1-8].html index.html
