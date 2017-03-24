# BitX40-RaduinoExtra

An enhancement to Asshar Farhan's Raduino software for the BitX40 transceiver. These are the main differences :-

1. Added support for straight key CW based on mod by Don Cantrell ND6Y (http://bitxhacks.blogspot.co.uk/2017/02/putting-bitx-raduino-on-cw.html)

2. Added support for 'graphical' S-meter based on mod by Don Cantrell (http://bitxhacks.blogspot.co.uk/2017/01/a-simple-s-meter-for-bitx-40-from-don.html) 

3. Implemented a 'tuning lock' using function button 2 
 
This is very much a work in progress and many features are currently untested. The code is built using interrupts to avoid the problems of having delays in the main loop(). Many thanks to Don for his great blog posts, original code and helpful support via email.

Andrew Whaley
