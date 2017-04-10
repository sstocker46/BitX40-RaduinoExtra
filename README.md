# BitX40-RaduinoExtra

An enhancement to Asshar Farhan's Raduino software for the BitX40 transceiver. These are the main differences :-

1. Added support for straight key CW based on mod by Don Cantrell ND6Y (http://bitxhacks.blogspot.co.uk/2017/02/putting-bitx-raduino-on-cw.html)

2. Added support for 'graphical' S-meter based on mod by Don Cantrell (http://bitxhacks.blogspot.co.uk/2017/01/a-simple-s-meter-for-bitx-40-from-don.html) 

3. Implemented a 'tuning lock' using function button 2 (for traditional tuning)

4. Added support for inverted i.e. upside down displays for idiots like me who fitted the display upside down and can't easily turn it around.

5. Implemented Don Cantrell's Shuttle tuning with a scanner option.

6. Reduced LCD updates to minimise RF noise generated in the update loop.
 
Many thanks to Don for his great blog posts, original code and helpful support via email and, of course, many thanks to Asshar for creating a cheap and hackable radio kit ! 

Note there are 2 macros at the top of the main module :-

UPSIDE_DOWN
SHUTTLE_TUNING 

uncomment these if you want either of these features. 

You'll need to include both modules when building the sketch so make sure you have two tabs in your project. 

Traditional tuning is Ashhar Farhan's original where turning the knob directly increases or decreases the frequency. When you get to the end of the scale the tuning increases or decreases automatically by a large amount. One drawback of this approach is that any tiny variation in the pot voltage causes the frequency to jump about. To limit this, I've implemented a 'tuning lock' where function button 2 freezes the tuning until pressed again. 

Shuttle tuning is Don's invention and does nothing in the center of the pot's range but increases or decreases by a small amount for a small turn and by a large anount the more you turn. This avoids any jumping when stationary and gives you the ability to have very fine control or move by huge amounts quite naturally. 

In shuttle tuning mode, function button 2 activates a 'scan mode' where the frequency automatically increments until a strong signal is found and then it stops. This obviously requires the s-meter to be implemented.


Andrew Whaley
