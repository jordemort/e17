#ifdef E_TYPEDEFS

/* enum for various event types */
typedef enum _E_Acpi_Type 
{
   E_ACPI_TYPE_UNKNOWN = 0,
   E_ACPI_TYPE_AC_ADAPTER,
   E_ACPI_TYPE_BATTERY,
   E_ACPI_TYPE_BUTTON,
   E_ACPI_TYPE_FAN,
   E_ACPI_TYPE_LID,
   E_ACPI_TYPE_POWER,
   E_ACPI_TYPE_PROCESSOR,
   E_ACPI_TYPE_SLEEP,
   E_ACPI_TYPE_THERMAL,
   E_ACPI_TYPE_VIDEO,
   E_ACPI_TYPE_WIFI
} E_Acpi_Type;

/* enum for acpi signals */
typedef enum _E_Acpi_Device_Signal 
{
   E_ACPI_DEVICE_SIGNAL_UNKNOWN,
   E_ACPI_DEVICE_SIGNAL_NOTIFY = 80,
   E_ACPI_DEVICE_SIGNAL_CHANGED = 82, // device added or removed
   E_ACPI_DEVICE_SIGNAL_AWAKE = 83,
   E_ACPI_DEVICE_SIGNAL_EJECT = 84
} E_Acpi_Device_Signal;

/* enum for lid status */
typedef enum _E_Acpi_Lid_Status 
{
   E_ACPI_LID_UNKNOWN,
   E_ACPI_LID_CLOSED,
   E_ACPI_LID_OPEN
} E_Acpi_Lid_Status;

/* struct used to pass to event handlers */
typedef struct _E_Event_Acpi E_Event_Acpi;

#else
# ifndef E_ACPI_H
#  define E_ACPI_H

struct _E_Event_Acpi 
{
   const char *device, *bus_id;
   int type, signal, status;
};

EAPI int e_acpi_init(void);
EAPI int e_acpi_shutdown(void);

extern EAPI int E_EVENT_ACPI_UNKNOWN;
extern EAPI int E_EVENT_ACPI_AC_ADAPTER;
extern EAPI int E_EVENT_ACPI_BATTERY;
extern EAPI int E_EVENT_ACPI_BUTTON;
extern EAPI int E_EVENT_ACPI_FAN;
extern EAPI int E_EVENT_ACPI_LID;
extern EAPI int E_EVENT_ACPI_POWER;
extern EAPI int E_EVENT_ACPI_PROCESSOR;
extern EAPI int E_EVENT_ACPI_SLEEP;
extern EAPI int E_EVENT_ACPI_THERMAL;
extern EAPI int E_EVENT_ACPI_VIDEO;
extern EAPI int E_EVENT_ACPI_WIFI;

# endif
#endif