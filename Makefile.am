
SUBDIRS = src
AUTOMAKE_OPTIONS = foreign

install-data-hook:
	( cd dist; ./install.sh -h "$(DESTDIR)$(prefix)" -m docker;)
	
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}
