# Top-level dispatcher. Pick the chip family with TARGET={f103|f411};
# defaults to f103 so a bare `make` keeps working as before.
TARGET ?= f103

all:
	$(MAKE) -f makefile.app TARGET=$(TARGET)
	$(MAKE) -f makefile.boot TARGET=$(TARGET)

app:
	$(MAKE) -f makefile.app TARGET=$(TARGET)

boot:
	$(MAKE) -f makefile.boot TARGET=$(TARGET)

clean:
	$(MAKE) -f makefile.app clean TARGET=$(TARGET)
	$(MAKE) -f makefile.boot clean TARGET=$(TARGET)