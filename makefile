all: 

build:
	gcc -O3 -o encoder_411086027.exe encoder_411086027.c -lm
	gcc -O3 -o decoder_411086027.exe decoder_411086027.c -lm

demo0:
	./encoder_411086027.exe 0 KimberlyNCat.bmp R.txt G.txt B.txt dim.txt
	./decoder_411086027.exe 0 ResKimberly.bmp R.txt G.txt B.txt dim.txt
	cmp KimberlyNCat.bmp ResKimberly.bmp

demo1:
	./encoder_411086027.exe 1 KimberlyNCat.bmp Qt_Y.txt Qt_Cb.txt Qt_Cr.txt dim.txt qF_Y.raw qF_Cb.raw qF_Cr.raw eF_Y.raw eF_Cb.raw eF_Cr.raw

clean:
	rm -f encoder_411086027.exe decoder_411086027.exe R.txt G.txt B.txt dim.txt ResKimberly.bmp


