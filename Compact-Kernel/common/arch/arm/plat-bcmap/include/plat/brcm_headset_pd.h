/* Header file for HEASET DRIVER */


struct brcm_headset_pd {
	int hsirq;
	int hsbirq;
	int hsgpio;
	void (*check_hs_state) (int *);
	short key_press_threshold;
	short key_3pole_threshold;
	short key1_threshold_l;
	short key1_threshold_u;
	short key2_threshold_l;
	short key2_threshold_u;
	short key3_threshold_l;
	short key3_threshold_u;	
};

