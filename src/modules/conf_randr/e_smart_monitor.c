#include "e.h"
#include "e_mod_main.h"
#include "e_smart_monitor.h"

#define RESISTANCE_THRESHOLD 5
#define RESIZE_SNAP_FUZZINESS 60
#define ROTATE_SNAP_FUZZINESS 45

/* local structures */
typedef struct _E_Smart_Data E_Smart_Data;
struct _E_Smart_Data
{
   /* object geometry */
   Evas_Coord x, y, w, h;

   /* object geometry before move started */
   Evas_Coord mx, my, mw, mh;

   /* visible flag */
   Eina_Bool visible : 1;

   /* resizing flag */
   Eina_Bool resizing : 1;

   /* rotating flag */
   Eina_Bool rotating : 1;

   /* connected flag */
   Eina_Bool connected : 1;

   /* moving flag */
   Eina_Bool moving : 1;

   /* layout object (this monitors parent) */
   Evas_Object *o_layout;

   /* base monitor object */
   Evas_Object *o_base;

   /* frame object */
   Evas_Object *o_frame;

   /* monitor stand object */
   Evas_Object *o_stand;

   /* livethumbnail for background image */
   Evas_Object *o_thumb;

   /* popup menu for resolutions */
   E_Menu *menu;

   /* crtc information */
   E_Randr_Crtc_Info *crtc;

   /* list of event handlers */
   Eina_List *hdls;

   /* current orientation */
   Ecore_X_Randr_Orientation orientation;

   /* current rotation */
   int rotation, start_rotation;

   /* current mode */
   Ecore_X_Randr_Mode_Info *mode;

   /* container number (for bg preview) */
   int con;

   /* zone number (for bg preview) */
   int zone;

   /* list of available modes */
   Eina_List *modes;

   /* min & max resolution */
   struct
     {
        int w, h;
     } min, max;
};

/* local function prototypes */
static void _e_smart_add(Evas_Object *obj);
static void _e_smart_del(Evas_Object *obj);
static void _e_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void _e_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h);
static void _e_smart_show(Evas_Object *obj);
static void _e_smart_hide(Evas_Object *obj);
static void _e_smart_clip_set(Evas_Object *obj, Evas_Object *clip);
static void _e_smart_clip_unset(Evas_Object *obj);
static Eina_Bool _e_smart_cb_bg_update(void *data, int type __UNUSED__, void *event);
static void _e_smart_cb_resize_mouse_in(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_resize_mouse_out(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_resize_start(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_resize_stop(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_rotate_mouse_in(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_rotate_mouse_out(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_rotate_start(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_rotate_stop(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_indicator_mouse_in(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_indicator_mouse_out(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_indicator_toggle(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);
static void _e_smart_cb_frame_mouse_move(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event);
static void _e_smart_cb_thumb_mouse_in(void *data __UNUSED__, Evas *evas __UNUSED__, Evas_Object *obj, void *event __UNUSED__);
static void _e_smart_cb_thumb_mouse_out(void *data __UNUSED__, Evas *evas __UNUSED__, Evas_Object *obj, void *event __UNUSED__);
static void _e_smart_cb_thumb_mouse_down(void *data, Evas *evas __UNUSED__, Evas_Object *obj, void *event);
static void _e_smart_cb_thumb_mouse_up(void *data, Evas *evas __UNUSED__, Evas_Object *obj, void *event);
static int _e_smart_cb_modes_sort(const void *data1, const void *data2);

static void _e_smart_monitor_rotate(E_Smart_Data *sd, void *event);
static void _e_smart_monitor_rotate_snap(Evas_Object *obj);
static void _e_smart_monitor_resize(E_Smart_Data *sd, Evas_Object *mon, void *event);
static void _e_smart_monitor_resize_snap(Evas_Object *obj, Ecore_X_Randr_Mode_Info *mode);
static void _e_smart_monitor_move(E_Smart_Data *sd, Evas_Object *mon, void *event);

static Ecore_X_Randr_Mode_Info *_e_smart_monitor_resolution_get(E_Smart_Data *sd, Evas_Coord width, Evas_Coord height);
static Ecore_X_Randr_Orientation _e_smart_monitor_orientation_get(int rotation);
static int _e_smart_monitor_rotation_get(Ecore_X_Randr_Orientation orient);

static E_Menu *_e_smart_monitor_menu_new(Evas_Object *obj);
static void _e_smart_monitor_menu_cb_end(void *data __UNUSED__, E_Menu *m);
static void _e_smart_monitor_menu_cb_resolution_pre(void *data, E_Menu *mn, E_Menu_Item *mi);
static void _e_smart_monitor_menu_cb_resolution_change(void *data, E_Menu *mn, E_Menu_Item *mi);

Evas_Object *
e_smart_monitor_add(Evas *evas)
{
   static Evas_Smart *smart = NULL;
   static const Evas_Smart_Class sc = 
     {
        "smart_monitor", EVAS_SMART_CLASS_VERSION,
        _e_smart_add, _e_smart_del, _e_smart_move, _e_smart_resize,
        _e_smart_show, _e_smart_hide, NULL, 
        _e_smart_clip_set, _e_smart_clip_unset, 
        NULL, NULL, NULL, NULL, NULL, NULL, NULL
     };

   if (!smart)
     if (!(smart = evas_smart_class_new(&sc))) 
       return NULL;

   return evas_object_smart_add(evas, smart);
}

void 
e_smart_monitor_layout_set(Evas_Object *obj, Evas_Object *layout)
{
   E_Smart_Data *sd;
   Evas_Coord mw, mh, mfw;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   sd->o_layout = layout;

   /* grab min size of frame */
   edje_object_size_min_get(sd->o_frame, &mfw, NULL);

   /* grab smallest resolution and convert to smallest canvas size */
   e_layout_coord_virtual_to_canvas(sd->o_layout, sd->min.w, sd->min.h, 
                                    &mw, &mh);

   /* if min resolution width is smaller than frame, then set 
    * object min width to frame width */
   if (mw < mfw) mw = mfw;

   evas_object_size_hint_min_set(obj, mw, mh);
}

void 
e_smart_monitor_crtc_set(Evas_Object *obj, E_Randr_Crtc_Info *crtc)
{
   E_Smart_Data *sd;
   Evas_Object *o;
   const char *bg = NULL;
   E_Container *con;
   E_Desk *desk;
   E_Zone *zone;
   Evas_Coord w, h;
   Eina_List *l;
   E_Randr_Output_Info *output;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   sd->crtc = crtc;
   if (!crtc) return;

   /* record the current rotation */
   sd->orientation = crtc->current_orientation;
   sd->rotation = _e_smart_monitor_rotation_get(sd->orientation);
   sd->mode = crtc->current_mode;

   EINA_LIST_FOREACH(crtc->outputs, l, output)
     {
        Eina_List *modes = NULL, *m = NULL;
        Ecore_X_Randr_Mode_Info *mode = NULL;
        E_Randr_Monitor_Info *monitor = NULL;
        const char *name = NULL;
        char buff[1024];

        printf("Output: %d %s\n", output->xid, output->name);

        if (output->crtc)
          modes = output->crtc->outputs_common_modes;
        else if (output->monitor)
          modes = output->monitor->modes;

        /* grab a copy of this monitor's modes, 
         * filtering out duplicate resolutions */
        EINA_LIST_FOREACH(modes, m, mode)
          {
             Ecore_X_Randr_Mode_Info *nmode = NULL;

             if ((nmode = eina_list_data_get(m->next)))
               {
                  if (!strcmp(mode->name, nmode->name))
                    continue;
               }

             sd->modes = eina_list_append(sd->modes, mode);
          }

        /* sort the mode list */
        sd->modes = eina_list_sort(sd->modes, 0, _e_smart_cb_modes_sort);

        /* NB: This is just a development printf to list modes.
         * Remove when dialog is complete */
        EINA_LIST_FOREACH(sd->modes, m, mode)
          {
             double rate = 0.0;

             if ((mode->hTotal) && (mode->vTotal))
               rate = ((float)mode->dotClock / 
                       ((float)mode->hTotal * (float)mode->vTotal));

             printf("\tMode: %d %dx%d @ %.1fHz\n", mode->xid, 
                    mode->width, mode->height, rate);
          }

        /* get the min resolution for this monitor */
        mode = eina_list_nth(sd->modes, 0);
        sd->min.w = mode->width;
        sd->min.h = mode->height;

        /* get the max resolution for this monitor */
        mode = eina_list_data_get(eina_list_last(sd->modes));
        sd->max.w = mode->width;
        sd->max.h = mode->height;

        /* set resolution label based on current mode */
        snprintf(buff, sizeof(buff), "%d x %d", 
                 output->crtc->current_mode->width, 
                 output->crtc->current_mode->height);
        edje_object_part_text_set(sd->o_frame, "e.text.resolution", buff);

        /* tell monitor object we are enabled/disabled */
        if (output->connection_status == 
            ECORE_X_RANDR_CONNECTION_STATUS_CONNECTED)
          {
             sd->connected = EINA_TRUE;
             edje_object_signal_emit(sd->o_base, "e,state,enabled", "e");
             edje_object_signal_emit(sd->o_frame, "e,state,enabled", "e");
          }
        else
          {
             sd->connected = EINA_FALSE;
             edje_object_signal_emit(sd->o_base, "e,state,disabled", "e");
             edje_object_signal_emit(sd->o_frame, "e,state,disabled", "e");
          }

        /* get and display monitor name if available */
        if ((monitor = output->monitor))
          {
             if (monitor->edid)
               {
                  name = 
                    ecore_x_randr_edid_display_name_get(monitor->edid, 
                                                        monitor->edid_length);
               }
          }

        if (!name) name = output->name;
        edje_object_part_text_set(sd->o_frame, "e.text.name", name);
     }

   /* get which desk this is based on monitor geometry */
   con = e_container_current_get(e_manager_current_get());
   zone = 
     e_container_zone_at_point_get(con, crtc->geometry.x, crtc->geometry.y);
   desk = e_desk_at_xy_get(zone, crtc->geometry.x, crtc->geometry.y);
   if (!desk) desk = e_desk_current_get(zone);

   sd->con = con->num;
   sd->zone = zone->num;

   /* get bg file for this screen */
   bg = e_bg_file_get(con->num, zone->num, desk->x, desk->y);

   /* set livethumb size */
   w = zone->w;
   h = (w * zone->h) / zone->w;
   e_livethumb_vsize_set(sd->o_thumb, w, h);

   /* set livethumb image */
   o = e_livethumb_thumb_get(sd->o_thumb);
   if (!o) o = edje_object_add(e_livethumb_evas_get(sd->o_thumb));
   edje_object_file_set(o, bg, "e/desktop/background");
   e_livethumb_thumb_set(sd->o_thumb, o);
}

E_Randr_Crtc_Info *
e_smart_monitor_crtc_get(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (!obj) return NULL;

   if (!(sd = evas_object_smart_data_get(obj)))
     return NULL;

   return sd->crtc;
}

void 
e_smart_monitor_crtc_geometry_get(Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   if (x) *x = 0;
   if (y) *y = 0;
   if (w) *w = 0;
   if (h) *h = 0;

   if (!sd->crtc) return;

   if (sd->crtc)
     {
        if (x) *x = sd->crtc->geometry.x;
        if (y) *y = sd->crtc->geometry.y;
        if (w) *w = sd->crtc->geometry.w;
        if (h) *h = sd->crtc->geometry.h;
     }
   /* else */
   /*   { */
   /*      if (sd->crtc->monitor) */
   /*        { */
   /*           if (w) *w = sd->crtc->monitor->size_mm.width; */
   /*           if (h) *h = sd->crtc->monitor->size_mm.height; */
   /*        } */
   /*   } */
}

void 
e_smart_monitor_move_geometry_get(Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w, Evas_Coord *h)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   if (x) *x = sd->mx;
   if (y) *y = sd->my;
   if (w) *w = sd->mw;
   if (h) *h = sd->mh;
}

Eina_Bool 
e_smart_monitor_moving_get(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return EINA_FALSE;

   return sd->moving;
}

/* local functions */
static void 
_e_smart_add(Evas_Object *obj)
{
   E_Smart_Data *sd;
   Evas *evas;

   if (!(sd = calloc(1, sizeof(E_Smart_Data)))) return;

   /* retrieve canvas once for reuse */
   evas = evas_object_evas_get(obj);

   /* create base object */
   sd->o_base = edje_object_add(evas);
   e_theme_edje_object_set(sd->o_base, "base/theme/widgets", 
                           "e/conf/randr/main/monitor");
   evas_object_smart_member_add(sd->o_base, obj);

   /* create monitor frame with preview */
   sd->o_frame = edje_object_add(evas);
   e_theme_edje_object_set(sd->o_frame, "base/theme/widgets", 
                           "e/conf/randr/main/frame");
   edje_object_part_swallow(sd->o_base, "e.swallow.frame", sd->o_frame);
   evas_object_event_callback_add(sd->o_frame, EVAS_CALLBACK_MOUSE_MOVE, 
                                  _e_smart_cb_frame_mouse_move, obj);

   /* create bg preview */
   sd->o_thumb = e_livethumb_add(evas);
   edje_object_part_swallow(sd->o_frame, "e.swallow.preview", sd->o_thumb);
   evas_object_event_callback_add(sd->o_thumb, EVAS_CALLBACK_MOUSE_IN, 
                                  _e_smart_cb_thumb_mouse_in, NULL);
   evas_object_event_callback_add(sd->o_thumb, EVAS_CALLBACK_MOUSE_OUT, 
                                  _e_smart_cb_thumb_mouse_out, NULL);
   evas_object_event_callback_add(sd->o_thumb, EVAS_CALLBACK_MOUSE_UP, 
                                  _e_smart_cb_thumb_mouse_up, obj);
   evas_object_event_callback_add(sd->o_thumb, EVAS_CALLBACK_MOUSE_DOWN, 
                                  _e_smart_cb_thumb_mouse_down, obj);

   /* create monitor stand */
   sd->o_stand = edje_object_add(evas);
   e_theme_edje_object_set(sd->o_stand, "base/theme/widgets", 
                           "e/conf/randr/main/stand");
   edje_object_part_swallow(sd->o_base, "e.swallow.stand", sd->o_stand);
   evas_object_stack_below(sd->o_stand, sd->o_frame);

   /* add callbacks for 'resize' edje signals */
   edje_object_signal_callback_add(sd->o_frame, "e,action,resize,in", "e", 
                                   _e_smart_cb_resize_mouse_in, NULL);
   edje_object_signal_callback_add(sd->o_frame, "e,action,resize,out", "e", 
                                   _e_smart_cb_resize_mouse_out, NULL);
   edje_object_signal_callback_add(sd->o_frame, "e,action,resize,start", "e", 
                                   _e_smart_cb_resize_start, obj);
   edje_object_signal_callback_add(sd->o_frame, "e,action,resize,stop", "e", 
                                   _e_smart_cb_resize_stop, obj);

   /* add callbacks for 'rotate' edje signals */
   edje_object_signal_callback_add(sd->o_frame, "e,action,rotate,in", "e", 
                                   _e_smart_cb_rotate_mouse_in, sd);
   edje_object_signal_callback_add(sd->o_frame, "e,action,rotate,out", "e", 
                                   _e_smart_cb_rotate_mouse_out, sd);
   edje_object_signal_callback_add(sd->o_frame, "e,action,rotate,start", "e", 
                                   _e_smart_cb_rotate_start, obj);
   edje_object_signal_callback_add(sd->o_frame, "e,action,rotate,stop", "e", 
                                   _e_smart_cb_rotate_stop, obj);

   /* add callback for indicator edje signals */
   edje_object_signal_callback_add(sd->o_frame, "e,action,indicator,in", "e", 
                                   _e_smart_cb_indicator_mouse_in, NULL);
   edje_object_signal_callback_add(sd->o_frame, "e,action,indicator,out", "e", 
                                   _e_smart_cb_indicator_mouse_out, NULL);
   edje_object_signal_callback_add(sd->o_frame, 
                                   "e,action,indicator,toggle", "e", 
                                   _e_smart_cb_indicator_toggle, sd);

   /* create event handlers */
   sd->hdls = 
     eina_list_append(sd->hdls, 
                      ecore_event_handler_add(E_EVENT_BG_UPDATE, 
                                              _e_smart_cb_bg_update, sd));

   evas_object_smart_data_set(obj, sd);
}

static void 
_e_smart_del(Evas_Object *obj)
{
   E_Smart_Data *sd;
   Ecore_Event_Handler *hdl;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   /* delete the menu if it exists */
   if (sd->menu) e_object_del(E_OBJECT(sd->menu));

   /* delete callbacks for 'resize' edje signals */
   edje_object_signal_callback_del(sd->o_frame, "e,action,resize,in", "e", 
                                   _e_smart_cb_resize_mouse_in);
   edje_object_signal_callback_del(sd->o_frame, "e,action,resize,out", "e", 
                                   _e_smart_cb_resize_mouse_out);
   edje_object_signal_callback_del(sd->o_frame, "e,action,resize,start", "e", 
                                   _e_smart_cb_resize_start);
   edje_object_signal_callback_del(sd->o_frame, "e,action,resize,stop", "e", 
                                   _e_smart_cb_resize_stop);

   /* delete callbacks for 'rotate' edje signals */
   edje_object_signal_callback_del(sd->o_frame, "e,action,rotate,in", "e", 
                                   _e_smart_cb_rotate_mouse_in);
   edje_object_signal_callback_del(sd->o_frame, "e,action,rotate,out", "e", 
                                   _e_smart_cb_rotate_mouse_out);
   edje_object_signal_callback_del(sd->o_frame, "e,action,rotate,start", "e", 
                                   _e_smart_cb_rotate_start);
   edje_object_signal_callback_del(sd->o_frame, "e,action,rotate,stop", "e", 
                                   _e_smart_cb_rotate_stop);

   /* delete callback for indicator edje signals */
   edje_object_signal_callback_del(sd->o_frame, "e,action,indicator,in", "e", 
                                   _e_smart_cb_indicator_mouse_in);
   edje_object_signal_callback_del(sd->o_frame, "e,action,indicator,out", "e", 
                                   _e_smart_cb_indicator_mouse_out);
   edje_object_signal_callback_del(sd->o_frame, 
                                   "e,action,indicator,toggle", "e", 
                                   _e_smart_cb_indicator_toggle);

   /* delete event handlers */
   EINA_LIST_FREE(sd->hdls, hdl)
     ecore_event_handler_del(hdl);

   /* delete the list of modes */
   if (sd->modes) eina_list_free(sd->modes);

   /* delete the monitor objects */
   if (sd->o_stand) evas_object_del(sd->o_stand);
   if (sd->o_thumb) evas_object_del(sd->o_thumb);
   if (sd->o_frame) evas_object_del(sd->o_frame);
   if (sd->o_base) evas_object_del(sd->o_base);

   E_FREE(sd);
   evas_object_smart_data_set(obj, NULL);
}

static void 
_e_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   if ((sd->x == x) && (sd->y == y)) return;
   sd->x = x;
   sd->y = y;
   evas_object_move(sd->o_base, x, y);
}

static void 
_e_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   if ((sd->w == w) && (sd->h == h)) return;
   sd->w = w;
   sd->h = h;
   evas_object_resize(sd->o_base, w, h);
}

static void 
_e_smart_show(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   if (sd->visible) return;
   evas_object_show(sd->o_base);
   sd->visible = EINA_TRUE;
}

static void 
_e_smart_hide(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   if (!sd->visible) return;
   evas_object_hide(sd->o_base);
   sd->visible = EINA_FALSE;
}

static void 
_e_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   evas_object_clip_set(sd->o_stand, clip);
   evas_object_clip_set(sd->o_frame, clip);
   evas_object_clip_set(sd->o_base, clip);
}

static void 
_e_smart_clip_unset(Evas_Object *obj)
{
   E_Smart_Data *sd;

   if (!(sd = evas_object_smart_data_get(obj)))
     return;

   evas_object_clip_unset(sd->o_stand);
   evas_object_clip_unset(sd->o_frame);
   evas_object_clip_unset(sd->o_base);
}

static Eina_Bool 
_e_smart_cb_bg_update(void *data, int type, void *event)
{
   E_Smart_Data *sd;
   E_Event_Bg_Update *ev;

   if (type != E_EVENT_BG_UPDATE) return ECORE_CALLBACK_PASS_ON;
   if (!(sd = data)) return ECORE_CALLBACK_PASS_ON;
   if (!sd->crtc) return ECORE_CALLBACK_PASS_ON;
   ev = event;

   if (((ev->container < 0) || (sd->con == ev->container)) && 
       ((ev->zone < 0) || (sd->zone == ev->zone)))
     {
        if (((ev->desk_x < 0) || (sd->crtc->geometry.x == ev->desk_x)) && 
            ((ev->desk_y < 0) || (sd->crtc->geometry.y == ev->desk_y)))
          {
             Evas_Object *o;
             const char *bg = NULL;

             /* background changed. grab new bg file and set thumbnail */
             bg = e_bg_file_get(sd->con, sd->zone, ev->desk_x, ev->desk_y);

             o = e_livethumb_thumb_get(sd->o_thumb);
             if (!o) o = edje_object_add(e_livethumb_evas_get(sd->o_thumb));
             edje_object_file_set(o, bg, "e/desktop/background");
             e_livethumb_thumb_set(sd->o_thumb, o);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void 
_e_smart_cb_resize_mouse_in(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Manager *man;

   man = e_manager_current_get();
   e_pointer_type_push(man->pointer, obj, "resize_br");
}

static void 
_e_smart_cb_resize_mouse_out(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Manager *man;

   man = e_manager_current_get();
   e_pointer_type_pop(man->pointer, obj, "resize_br");
}

static void 
_e_smart_cb_resize_start(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Evas_Object *mon;
   E_Smart_Data *sd;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   sd->resizing = EINA_TRUE;
   e_layout_child_raise(mon);
}

static void 
_e_smart_cb_resize_stop(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Evas_Object *mon;
   E_Smart_Data *sd;
   Evas_Coord ow, oh;
   Ecore_X_Randr_Mode_Info *mode = NULL;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   sd->resizing = EINA_FALSE;
   e_layout_child_lower(mon);

   /* get the object geometry */
   if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_90) || 
       (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_270))
     {
        e_layout_child_geometry_get(mon, NULL, NULL, &oh, &ow);
        mode = _e_smart_monitor_resolution_get(sd, ow, oh);
     }
   else if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_0) || 
            (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_180))
     {
        e_layout_child_geometry_get(mon, NULL, NULL, &ow, &oh);
        mode = _e_smart_monitor_resolution_get(sd, ow, oh);
     }

   /* find the closest resolution to this one and snap to it */
   if (mode)
     {
        char buff[1024];

        /* set resolution text */
        snprintf(buff, sizeof(buff), "%d x %d", mode->width, mode->height);
        edje_object_part_text_set(sd->o_frame, "e.text.resolution", buff);

        /* actually snap the object */
        _e_smart_monitor_resize_snap(mon, mode);
     }
}

static void 
_e_smart_cb_rotate_mouse_in(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Smart_Data *sd;
   Ecore_Evas *ee;
   Ecore_X_Window win;
   Ecore_X_Cursor cur;

   if (!(sd = data)) return;

   /* changing cursors for rotate is done this way because e_pointer 
    * does not support all available X cursors */
   ee = ecore_evas_ecore_evas_get(evas_object_evas_get(sd->o_frame));
   win = (Ecore_X_Window)ecore_evas_window_get(ee);

   cur = ecore_x_cursor_shape_get(ECORE_X_CURSOR_EXCHANGE);
   ecore_x_window_cursor_set(win, cur);
   ecore_x_cursor_free(cur);
}

static void 
_e_smart_cb_rotate_mouse_out(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Smart_Data *sd;
   Ecore_Evas *ee;
   Ecore_X_Window win;

   if (!(sd = data)) return;

   /* reset cursor back to default */
   ee = ecore_evas_ecore_evas_get(evas_object_evas_get(sd->o_frame));
   win = (Ecore_X_Window)ecore_evas_window_get(ee);
   ecore_x_window_cursor_set(win, 0);
}

static void 
_e_smart_cb_rotate_start(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Evas_Object *mon;
   E_Smart_Data *sd;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   sd->rotating = EINA_TRUE;
   sd->rotation = 0;
   sd->start_rotation = _e_smart_monitor_rotation_get(sd->orientation);

   e_layout_child_raise(mon);
}

static void 
_e_smart_cb_rotate_stop(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   Evas_Object *mon;
   E_Smart_Data *sd;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   sd->rotating = EINA_FALSE;
   e_layout_child_lower(mon);

   /* accumulate rotation values from start */
   sd->rotation += sd->start_rotation;

   /* get closest orientation to this one */
   sd->orientation = _e_smart_monitor_orientation_get(sd->rotation);

   /* set rotation to be the angle of this orientation */
   sd->rotation = _e_smart_monitor_rotation_get(sd->orientation);

   if (sd->orientation >= ECORE_X_RANDR_ORIENTATION_ROT_0)
     {
        Evas_Coord x, y, w, h;
        Evas_Map *map;

        evas_object_geometry_get(sd->o_frame, &x, &y, &w, &h);

        /* create frame 'map' for rotation */
        map = evas_map_new(4);
        evas_map_smooth_set(map, EINA_TRUE);
        evas_map_alpha_set(map, EINA_TRUE);
        evas_map_util_points_populate_from_object_full(map, sd->o_frame, 
                                                      sd->rotation);
        evas_map_util_rotate(map, sd->rotation,
                             x + (w / 2), y + (h / 2));
        evas_object_map_set(sd->o_frame, map);
        evas_object_map_enable_set(sd->o_frame, EINA_TRUE);
        evas_map_free(map);

        /* actually snap the object */
        _e_smart_monitor_rotate_snap(mon);
     }
}

static void 
_e_smart_cb_indicator_mouse_in(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Manager *man;

   man = e_manager_current_get();
   e_pointer_type_push(man->pointer, obj, "hand");
}

static void 
_e_smart_cb_indicator_mouse_out(void *data __UNUSED__, Evas_Object *obj, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Manager *man;

   man = e_manager_current_get();
   e_pointer_type_pop(man->pointer, obj, "hand");
}

static void 
_e_smart_cb_indicator_toggle(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Smart_Data *sd;

   if (!(sd = data)) return;

   if (sd->connected)
     {
        sd->connected = EINA_FALSE;
        edje_object_signal_emit(sd->o_base, "e,state,disabled", "e");
        edje_object_signal_emit(sd->o_frame, "e,state,disabled", "e");
     }
   else
     {
        sd->connected = EINA_TRUE;
        edje_object_signal_emit(sd->o_base, "e,state,enabled", "e");
        edje_object_signal_emit(sd->o_frame, "e,state,enabled", "e");
     }
}

static void 
_e_smart_cb_frame_mouse_move(void *data, Evas *evas __UNUSED__, Evas_Object *obj __UNUSED__, void *event)
{
   Evas_Object *mon;
   E_Smart_Data *sd;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   /* if we are not rotating or resizing, then we have nothing to do */
   if ((!sd->rotating) && (!sd->resizing) && (!sd->moving)) return;

   if (sd->rotating) 
     _e_smart_monitor_rotate(sd, event);
   else if (sd->resizing) 
     _e_smart_monitor_resize(sd, mon, event);
   else if (sd->moving)
     _e_smart_monitor_move(sd, mon, event);
}

static void 
_e_smart_cb_thumb_mouse_in(void *data __UNUSED__, Evas *evas __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   E_Manager *man;

   man = e_manager_current_get();
   e_pointer_type_push(man->pointer, obj, "hand");
}

static void 
_e_smart_cb_thumb_mouse_out(void *data __UNUSED__, Evas *evas __UNUSED__, Evas_Object *obj, void *event __UNUSED__)
{
   E_Manager *man;

   man = e_manager_current_get();
   e_pointer_type_pop(man->pointer, obj, "hand");
}

static void 
_e_smart_cb_thumb_mouse_down(void *data, Evas *evas __UNUSED__, Evas_Object *obj, void *event)
{
   Evas_Object *mon;
   E_Smart_Data *sd;
   Evas_Event_Mouse_Up *ev;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   ev = event;
   if (ev->button == 1)
     {
        E_Manager *man;

        man = e_manager_current_get();
        e_pointer_type_push(man->pointer, obj, "move");

        /* record this monitors geometry before moving */
        e_layout_child_geometry_get(mon, &sd->mx, &sd->my, &sd->mw, &sd->mh);

        /* update moving state */
        sd->moving = EINA_TRUE;

        e_layout_child_raise(mon);
     }
}

static void 
_e_smart_cb_thumb_mouse_up(void *data, Evas *evas __UNUSED__, Evas_Object *obj, void *event)
{
   Evas_Object *mon;
   E_Smart_Data *sd;
   Evas_Event_Mouse_Up *ev;

   if (!(mon = data)) return;
   if (!(sd = evas_object_smart_data_get(mon))) return;

   ev = event;
   if (ev->button == 3)
     {
        Ecore_X_Window root;
        Evas_Coord px, py;

        root = ecore_x_window_root_first_get();
        ecore_x_pointer_xy_get(root, &px, &py);

        /* if we have the menu, show it */
        if (sd->menu)
          {
             E_Zone *zone;

             zone = e_util_zone_current_get(e_manager_current_get());
             e_menu_activate(sd->menu, zone, px, py, 1, 1, 
                             E_MENU_POP_DIRECTION_DOWN);
          }
        else
          {
             /* create and show the resolution popup menu */
             if ((sd->menu = _e_smart_monitor_menu_new(mon)))
               {
                  E_Zone *zone;

                  zone = e_util_zone_current_get(e_manager_current_get());
                  e_menu_activate(sd->menu, zone, 
                                  px, py, 1, 1, 
                                  E_MENU_POP_DIRECTION_DOWN);
               }
          }
     }
   else if (ev->button == 1)
     {
        E_Manager *man;

        man = e_manager_current_get();
        e_pointer_type_pop(man->pointer, obj, "move");

        /* update moving state */
        sd->moving = EINA_FALSE;

        /* tell randr widget that we moved this monitor */
        evas_object_smart_callback_call(mon, "monitor_moved", NULL);
     }
}

static int 
_e_smart_cb_modes_sort(const void *data1, const void *data2)
{
   const Ecore_X_Randr_Mode_Info *m1, *m2 = NULL;

   if (!(m1 = data1)) return 1;
   if (!(m2 = data2)) return -1;

   /* second one compares to previous to determine position */
   if (m2->width < m1->width) return 1;
   if (m2->width > m1->width) return -1;

   /* width are same, compare heights */
   if ((m2->width == m1->width))
     {
        if (m2->height < m1->height) return 1;
        if (m2->height > m1->height) return -1;
     }

   return 1;
}

static void 
_e_smart_monitor_rotate(E_Smart_Data *sd, void *event)
{
   Evas_Event_Mouse_Move *ev;
   Evas_Coord mx;
   Evas_Coord x, y, w, h;
   Evas_Map *map;

   ev = event;

   mx = (ev->prev.output.x - ev->cur.output.x);
   evas_object_geometry_get(sd->o_frame, &x, &y, &w, &h);

   /* create frame 'map' for rotation */
   map = evas_map_new(4);
   evas_map_smooth_set(map, EINA_TRUE);
   evas_map_alpha_set(map, EINA_TRUE);
   evas_map_util_points_populate_from_object_full(map, sd->o_frame, 
                                                 sd->rotation);
   evas_map_util_rotate(map, sd->rotation + mx, x + (w / 2), y + (h / 2));
   evas_object_map_set(sd->o_frame, map);
   evas_object_map_enable_set(sd->o_frame, EINA_TRUE);
   evas_map_free(map);

   sd->rotation += mx;
}

static void 
_e_smart_monitor_rotate_snap(Evas_Object *obj)
{
   E_Smart_Data *sd;
   Evas_Coord nw, nh;

   if (!(sd = evas_object_smart_data_get(obj))) return;

   nw = sd->mode->width;
   nh = sd->mode->height;

   /* get the object geometry */
   if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_90) || 
       (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_270))
     e_layout_child_resize(obj, nh, nw);
   else if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_0) || 
            (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_180))
     e_layout_child_resize(obj, nw, nh);

   /* tell randr widget we rotated this monitor so that it can 
    * update the layout for any monitors around this one */
   evas_object_smart_callback_call(obj, "monitor_rotated", NULL);
}

static void 
_e_smart_monitor_resize(E_Smart_Data *sd, Evas_Object *mon, void *event)
{
   Evas_Event_Mouse_Move *ev;
   Evas_Coord w, h, cw, ch;
   Evas_Coord mx, my;
   Evas_Coord nrw, nrh;
   Ecore_X_Randr_Mode_Info *mode = NULL;

   ev = event;

   /* calculate resize difference */
   mx = (ev->cur.output.x - ev->prev.output.x);
   my = (ev->cur.output.y - ev->prev.output.y);

   /* grab size of monitor object and convert to canvas coords */
   if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_90) || 
       (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_270))
     {
        e_layout_child_geometry_get(mon, NULL, NULL, &ch, &cw);
        e_layout_coord_virtual_to_canvas(sd->o_layout, ch, cw, &h, &w);

        e_layout_coord_canvas_to_virtual(sd->o_layout, 
                                         (h + mx), (w + my), &nrh, &nrw);

        /* determine if this new size is below or above the 
         * available resolutions and stop resizing if so */
        if ((nrw < sd->min.w) || (nrh < sd->min.h)) return;
        if ((nrw > sd->max.w) || (nrh > sd->max.h)) return;

        e_layout_child_resize(mon, nrh, nrw);

        /* find the closest resolution to this one that we would snap to */
        mode = _e_smart_monitor_resolution_get(sd, nrw, nrh);
     }
   else if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_0) || 
            (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_180))
     {
        e_layout_child_geometry_get(mon, NULL, NULL, &cw, &ch);
        e_layout_coord_virtual_to_canvas(sd->o_layout, cw, ch, &w, &h);
        e_layout_coord_canvas_to_virtual(sd->o_layout, 
                                         (w + mx), (h + my), &nrw, &nrh);

        /* determine if this new size is below or above the 
         * available resolutions and stop resizing if so */
        if ((nrw < sd->min.w) || (nrh < sd->min.h)) return;
        if ((nrw > sd->max.w) || (nrh > sd->max.h)) return;

        e_layout_child_resize(mon, nrw, nrh);

        /* find the closest resolution to this one that we would snap to */
        mode = _e_smart_monitor_resolution_get(sd, nrw, nrh);
     }

   /* tell randr widget we resized this monitor so that it can 
    * update the layout for any monitors around this one */
   /* evas_object_smart_callback_call(mon, "monitor_resized", NULL); */

   if (mode)
     {
        char buff[1024];

        /* set resolution text */
        snprintf(buff, sizeof(buff), "%d x %d", mode->width, mode->height);
        edje_object_part_text_set(sd->o_frame, "e.text.resolution", buff);
     }
}

static void 
_e_smart_monitor_resize_snap(Evas_Object *obj, Ecore_X_Randr_Mode_Info *mode)
{
   E_Smart_Data *sd;
   char buff[1024];

   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* set this to the current mode */
   sd->mode = mode;

   /* resize the child object */
   if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_90) || 
       (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_270))
     e_layout_child_resize(obj, mode->height, mode->width);
   else if ((sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_0) || 
            (sd->orientation == ECORE_X_RANDR_ORIENTATION_ROT_180))
     e_layout_child_resize(obj, mode->width, mode->height);

   /* set resolution text */
   snprintf(buff, sizeof(buff), "%d x %d", mode->width, mode->height);
   edje_object_part_text_set(sd->o_frame, "e.text.resolution", buff);

   /* tell randr widget we resized this monitor so that it can 
    * update the layout for any monitors around this one */
   evas_object_smart_callback_call(obj, "monitor_resized", NULL);
}

static void 
_e_smart_monitor_move(E_Smart_Data *sd, Evas_Object *mon, void *event)
{
   Evas_Event_Mouse_Move *ev;
   Evas_Coord px, py, pw, ph;
   Evas_Coord gx, gy, gw, gh;
   Evas_Coord dx, dy;
   Evas_Coord nx, ny;

   if (!sd) return;
   ev = event;

   /* grab size of layout widget */
   evas_object_geometry_get(sd->o_layout, &px, &py, NULL, NULL);
   e_layout_virtual_size_get(sd->o_layout, &pw, &ph);

   /* account for mouse movement */
   dx = (ev->cur.canvas.x - ev->prev.canvas.x);
   dy = (ev->cur.canvas.y - ev->prev.canvas.y);

   /* convert coordinates to virtual space */
   e_layout_coord_canvas_to_virtual(sd->o_layout, (px + dx), (py + dy), 
                                    &nx, &ny);

   /* get current monitor geometry */
   e_layout_child_geometry_get(mon, &gx, &gy, &gw, &gh);
   nx += gx;
   ny += gy;

   /* make sure we do not move beyond the layout bounds */
   if (nx < px) nx = px;
   else if (nx > px + pw - gw)
     nx = px + pw - gw;

   if (ny < py) ny = py;
   else if (ny > py + ph - gh)
     ny = py + ph - gh;

   /* actually move the monitor */
   if ((gx != nx) || (gy != ny))
     {
        e_layout_child_move(mon, nx, ny);
        evas_object_smart_callback_call(mon, "monitor_moved", NULL);
     }

   /* Hmm, this below code worked also ... and seems lighter.
    * but the above code seems more "proper".
    * Which to use ?? */

   /* Evas_Coord mx, my; */
   /* Evas_Coord nx, ny; */

   /* get current monitor position */
   /* evas_object_geometry_get(mon, &mx, &my, NULL, NULL); */

   /* update for mouse movement */
   /* mx = mx + (ev->cur.output.x - ev->prev.output.x); */
   /* my = my + (ev->cur.output.y - ev->prev.output.y); */

   /* convert to virtual coordinates */
   /* e_layout_coord_canvas_to_virtual(sd->o_layout, mx, my, &nx, &ny); */

   /* actually move the monitor */
   /* e_layout_child_move(mon, nx, ny); */
}

static Ecore_X_Randr_Mode_Info *
_e_smart_monitor_resolution_get(E_Smart_Data *sd, Evas_Coord width, Evas_Coord height)
{
   Ecore_X_Randr_Mode_Info *mode;
   Eina_List *l;

   if (!sd) return NULL;

   /* find the closest resolution we have, within 'fuzziness' range */
   EINA_LIST_REVERSE_FOREACH(sd->modes, l, mode)
     {
        if ((((int)mode->width - RESIZE_SNAP_FUZZINESS) <= width) || 
            (((int)mode->width + RESIZE_SNAP_FUZZINESS) <= width))
          {
             if ((((int)mode->height - RESIZE_SNAP_FUZZINESS) <= height) || 
                 (((int)mode->height + RESIZE_SNAP_FUZZINESS) <= height))
               return mode;
          }
     }

   return NULL;
}

static Ecore_X_Randr_Orientation 
_e_smart_monitor_orientation_get(int rotation)
{
   if (rotation < 0) rotation += 360;
   else if (rotation > 360) rotation -= 360;

   /* find the closest rotation of rotation within 'fuzziness' tolerance */
   if (((rotation - ROTATE_SNAP_FUZZINESS) <= 0) || 
       ((rotation + ROTATE_SNAP_FUZZINESS) <= 0))
     return ECORE_X_RANDR_ORIENTATION_ROT_0;
   else if (((rotation - ROTATE_SNAP_FUZZINESS) <= 90) || 
       ((rotation + ROTATE_SNAP_FUZZINESS) <= 90))
     return ECORE_X_RANDR_ORIENTATION_ROT_90;
   else if (((rotation - ROTATE_SNAP_FUZZINESS) <= 180) || 
            ((rotation + ROTATE_SNAP_FUZZINESS) <= 180))
     return ECORE_X_RANDR_ORIENTATION_ROT_180;
   else if (((rotation - ROTATE_SNAP_FUZZINESS) <=  270) || 
            ((rotation + ROTATE_SNAP_FUZZINESS) <= 270))
     return ECORE_X_RANDR_ORIENTATION_ROT_270;
   else if (((rotation - ROTATE_SNAP_FUZZINESS) <= 360) || 
            ((rotation + ROTATE_SNAP_FUZZINESS) <= 360))
     return ECORE_X_RANDR_ORIENTATION_ROT_0;

   return -1;
}

static int 
_e_smart_monitor_rotation_get(Ecore_X_Randr_Orientation orient)
{
   switch (orient)
     {
      case ECORE_X_RANDR_ORIENTATION_ROT_90:
        return 90;
      case ECORE_X_RANDR_ORIENTATION_ROT_180:
        return 180;
      case ECORE_X_RANDR_ORIENTATION_ROT_270:
        return 270;
      case ECORE_X_RANDR_ORIENTATION_ROT_0:
      default:
        return 0;
     }
}

static E_Menu *
_e_smart_monitor_menu_new(Evas_Object *obj)
{
   E_Smart_Data *sd = NULL;
   E_Menu *m;
   E_Menu_Item *mi = NULL;

   if (!(sd = evas_object_smart_data_get(obj))) return NULL;

   /* create the base menu */
   m = e_menu_new();
   e_menu_category_set(m, "monitor");
   e_menu_category_data_set("monitor", sd);
   e_object_data_set(E_OBJECT(m), sd);

   /* add deactivate callback on menu for cleanup */
   e_menu_post_deactivate_callback_set(m, _e_smart_monitor_menu_cb_end, NULL);

   /* create resolution entry */
   if ((mi = e_menu_item_new(m)))
     {
        e_menu_item_label_set(mi, _("Resolution"));
        e_util_menu_item_theme_icon_set(mi, 
                                        "preferences-system-screen-resolution");
        e_menu_item_submenu_pre_callback_set(mi, 
                                             _e_smart_monitor_menu_cb_resolution_pre, 
                                             obj);
     }

   return m;
}

static void 
_e_smart_monitor_menu_cb_end(void *data __UNUSED__, E_Menu *m)
{
   E_Smart_Data *sd;

   if ((sd = e_object_data_get(E_OBJECT(m))))
     {
        e_object_del(E_OBJECT(sd->menu));
        sd->menu = NULL;
     }
   else
     e_object_del(E_OBJECT(m));
}

static void 
_e_smart_monitor_menu_cb_resolution_pre(void *data, E_Menu *mn, E_Menu_Item *mi)
{
   Evas_Object *obj = NULL;
   E_Smart_Data *sd = NULL;
   E_Menu *subm = NULL;
   Eina_List *m = NULL;
   Ecore_X_Randr_Mode_Info *mode = NULL;

   if (!(obj = data)) return;
   if (!(sd = e_object_data_get(E_OBJECT(mn)))) return;

   /* create resolution submenu */
   subm = e_menu_new();
   e_object_data_set(E_OBJECT(subm), sd);
   e_menu_item_submenu_set(mi, subm);
   e_object_unref(E_OBJECT(subm));

   /* loop the list of Modes */
   EINA_LIST_FOREACH(sd->modes, m, mode)
     {
        E_Menu_Item *submi = NULL;

        /* create menu item for this mode, and set the label */
        submi = e_menu_item_new(subm);
        e_menu_item_label_set(submi, mode->name);
        e_menu_item_radio_set(submi, EINA_TRUE);
        e_menu_item_radio_group_set(submi, 1);
        e_menu_item_callback_set(submi, 
                                 _e_smart_monitor_menu_cb_resolution_change, 
                                 obj);

        /* if this is the current mode, mark menu item as selected */
        if ((mode->width == sd->crtc->current_mode->width) && 
            (mode->height == sd->crtc->current_mode->height))
          e_menu_item_toggle_set(submi, EINA_TRUE);
     }
}

static void 
_e_smart_monitor_menu_cb_resolution_change(void *data, E_Menu *mn, E_Menu_Item *mi)
{
   Evas_Object *obj = NULL;
   E_Smart_Data *sd = NULL;
   Eina_List *m = NULL;
   Ecore_X_Randr_Mode_Info *mode = NULL;

   if (!(obj = data)) return;
   if (!(sd = e_object_data_get(E_OBJECT(mn)))) return;

   /* loop the list of Modes */
   EINA_LIST_FOREACH(sd->modes, m, mode)
     {
        if ((mi->label) && (mode->name))
          {
             /* compare mode name to menu item label */
             if (!strcmp(mode->name, mi->label))
               {
                  /* found requested mode, set it */
                  _e_smart_monitor_resize_snap(obj, mode);
                  break;
               }
          }
     }
}
