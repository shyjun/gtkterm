all:
	meson build/
	ninja -C build/

clean:
	rm build/ -rf
	rm -rfv ./new_gtkterm

run:
	./build/src/gtkterm

cpy:
	cp build/src/gtkterm ./new_gtkterm -v
