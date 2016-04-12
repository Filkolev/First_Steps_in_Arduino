// Serial communication
#define BAUD_RATE 9600

// Motor off signal frequency (31 - 65535)
#define MOTOR_OFF_FREQUENCY 50000

// Random inflow max value
#define RANDOM_INFLOW_MAX 1000

// Analog and digital read/write value limits
#define MIN_ANALOG_READ 0
#define MAX_ANALOG_READ 1023
#define MIN_ANALOG_WRITE 0
#define MAX_ANALOG_WRITE 255

// Motor speed definitions
#define ZERO_SPEED 0
#define QUARTER_SPEED 64
#define HALF_SPEED 128
#define THREE_QUARTERS_SPEED 191
#define FULL_SPEED 255

// Motor controller levels to modify speed
#define QUARTER_SPEED_CTRL_MAX 256
#define HALF_SPEED_CTRL_MAX 512
#define THREE_QUARTERS_SPEED_CTRL_MAX 767

// Pool status
#define POOL_EMPTY 0
#define POOL_LOW_LOWER 500000
#define POOL_LOW_HIGHER 1200000
#define POOL_HIGH_LOWER 2500000
#define POOL_CRITICAL_LOWER 3500000
#define POOL_FULL_LOWER 4000000
#define POOL_FULL 5000000
#define INFLOW_ABS_MAX 150000

// Timeouts
#define DEBOUNCE_INTERVAL 50
#define LOG_INFO_INTERVAL 3000
#define RANDOM_INFLOW_INTERVAL 1000
#define LOW_ENERGY_TIMEOUT 1000
#define OK_ENERGY_TIMEOUT 1000
#define HIGH_ENERGY_TIMEOUT 500
#define CRITICAL_ENERGY_TIMEOUT 250
#define LOW_POWER_MODE_LOG_TIMEOUT 50
#define STABILITY_TIMEOUT 25
