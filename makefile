all: 

build:
	gcc -O3 -o encoder_411086027.exe encoder_411086027.c -lm
	gcc -O3 -o decoder_411086027.exe decoder_411086027.c -lm

demo0:
	./encoder_411086027.exe 0 KimberlyNCat.bmp R.txt G.txt B.txt dim.txt
	./decoder_411086027.exe 0 ResKimberly.bmp R.txt G.txt B.txt dim.txt
	cmp KimberlyNCat.bmp ResKimberly.bmp

clean:
	rm -f encoder_411086027.exe decoder_411086027.exe R.txt G.txt B.txt dim.txt ResKimberly.bmp


