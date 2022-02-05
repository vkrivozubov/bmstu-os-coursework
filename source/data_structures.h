struct mouse_event_info {
	char buttons;
	char dx;
	char dy;
	char wheel;
};

typedef struct mouse_event_info mouse_event_data;

struct mouse_data_package {
	mouse_event_data *info;
	int size;
};

typedef struct mouse_data_package mouse_data_package;

struct keyboard_event_info {
	char description[25];
};

typedef struct keyboard_event_info keyboard_event_data;

struct keyboard_data_package {
	keyboard_event_data *info;
	int size;
};

typedef struct keyboard_data_package keyboard_data_package;

struct process {
    char *name;
    int secs_in_use;
};

typedef struct process process;
