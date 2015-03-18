all:
	make -C ./daemon
	make -C ./driver
	make -C ./initd
clean:
	make clean -C ./driver
	make clean -C ./daemon
	make clean -C ./initd

install:
	install -d /usr/local/advtty
	cp ./driver/advvcom.ko /usr/local/advtty/
	cp ./daemon/vcomd /usr/local/advtty/
	cp ./initd/advttyd /usr/local/advtty/ 
	cp ./config/advttyd.conf /usr/local/advtty/
	cp ./Makefile  /usr/local/advtty/
	cp ./script/advls /usr/local/advtty/
	cp ./script/advadd /usr/local/advtty/
	cp ./script/advrm /usr/local/advtty/
	cp ./script/advman /usr/local/advtty/
	chmod 111 /usr/local/advtty/advls
	chmod 111 /usr/local/advtty/advadd
	chmod 111 /usr/local/advtty/advrm
	chmod 111 /usr/local/advtty/advman
	ln -sf /usr/local/advtty/advls /sbin/advls
	ln -sf /usr/local/advtty/advrm /sbin/advrm
	ln -sf /usr/local/advtty/advadd /sbin/advadd
	ln -sf /usr/local/advtty/advman /sbin/advman

uninstall:
	rm -Rf /usr/local/advtty
	rm -f /sbin/advrm
	rm -f /sbin/advls
	rm -f /sbin/advman
	rm -f /sbin/advadd
