{
	uint8 : 0xff;
	uint16: 0xffff;
	uint32: 0xffffffff;
	uint64: 0xffffffffffffffff;
	
	sint8 : -(uint8  >> 1) = -0;
	sint16: -(uint16 >> 1);
	sint32: -(uint32 >> 1);
	sint64: -(uint64 >> 1);

	void: ();
	
	real32: 0.0 = 0.;
	real64: 0.0;
}
