{
	uint8 : 0xff;
	uint16: 0xffff;
	uint32: 0xffffffff;
	uint64: 0xffffffffffffffff;
	
	sint8 : -(0xff               >> 1) = -0;
	sint16: -(0xffff             >> 1);
	sint32: -(0xffffffff         >> 1);
	sint64: -(0xffffffffffffffff >> 1);

	null: ();
	
	real32: 0.0 = 0.;
	real64: 0.0;
}
