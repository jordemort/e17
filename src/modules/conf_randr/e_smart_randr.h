#ifdef E_TYPEDEFS
#else
# ifndef E_SMART_RANDR_H
#  define E_SMART_RANDR_H

Evas_Object *e_smart_randr_add(Evas *evas);
void e_smart_randr_virtual_size_set(Evas_Object *obj, Evas_Coord vw, Evas_Coord vh);
void e_smart_randr_monitor_add(Evas_Object *obj, Evas_Object *mon);

# endif
#endif
