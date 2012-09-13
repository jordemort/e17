#include "e.h"
#include "e_fm_device.h"
#include "e_mod_main.h"
#include "e_mod_config.h"
#include "e_mod_dbus.h"

/* actual module specifics */
static void _e_mod_action_fileman_cb(E_Object   *obj,
                                     const char *params);
static E_Menu *_e_mod_menu_top_get(E_Menu *m);
static void _e_mod_menu_gtk_cb(void        *data,
                               E_Menu      *m,
                               E_Menu_Item *mi);
static void _e_mod_menu_virtual_cb(void        *data,
                                   E_Menu      *m,
                                   E_Menu_Item *mi);
static void _e_mod_menu_volume_cb(void        *data,
                                  E_Menu      *m,
                                  E_Menu_Item *mi);
static void      _e_mod_main_menu_cb(E_Menu *m, void *category_data, void *data);
static void      _e_mod_menu_populate(void *d __UNUSED__, E_Menu *m, E_Menu_Item *mi);
static void      _e_mod_menu_cleanup_cb(void *obj);
static void      _e_mod_menu_add(void   *data,
                                 E_Menu *m);
static void      _e_mod_fileman_config_load(void);
static void      _e_mod_fileman_config_free(void);
static Eina_Bool _e_mod_zone_add(void *data,
                                 int   type,
                                 void *event);

static E_Module *conf_module = NULL;
static E_Action *act = NULL;
static E_Int_Menu_Augmentation *maug = NULL;
static E_Menu_Category_Callback *mcb = NULL;
static Ecore_Event_Handler *zone_add_handler = NULL;

static E_Config_DD *paths_edd = NULL, *conf_edd = NULL;
Config *fileman_config = NULL;

/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Fileman"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   Eina_List *l, *ll, *lll;
   E_Manager *man;
   E_Container *con;
   E_Zone *zone;

   conf_module = m;

   //eina_init();

   /* Setup Entry in Config Panel */
   e_configure_registry_category_add("fileman", 100, _("Files"),
                                     NULL, "system-file-manager");
   e_configure_registry_item_add("fileman/fileman", 10, _("File Manager"),
                                 NULL, "system-file-manager",
                                 e_int_config_fileman);
   e_configure_registry_item_add("fileman/file_icons", 20, _("File Icons"),
                                 NULL, "preferences-file-icons",
                                 e_int_config_mime);
   /* Setup Config edd */
   _e_mod_fileman_config_load();

   /* add module supplied action */
   act = e_action_add("fileman");
   if (act)
     {
        act->func.go = _e_mod_action_fileman_cb;
        e_action_predef_name_set(_("Launch"), _("File Manager"),
                                 "fileman", NULL, "syntax: /path/to/dir or ~/path/to/dir or favorites or desktop, examples: /boot/grub, ~/downloads", 1);
     }
   maug = e_int_menus_menu_augmentation_add_sorted("main/1", _("Navigate"), _e_mod_menu_add, NULL, NULL, NULL);
   mcb = e_menu_category_callback_add("e/fileman/action", _e_mod_main_menu_cb, NULL, NULL);
   e_module_delayed_set(m, 1);

   /* Hook into zones */
   for (l = e_manager_list(); l; l = l->next)
     {
        man = l->data;
        for (ll = man->containers; ll; ll = ll->next)
          {
             con = ll->data;
             for (lll = con->zones; lll; lll = lll->next)
               {
                  zone = lll->data;
                  if (e_fwin_zone_find(zone)) continue;
                  if (fileman_config->view.show_desktop_icons)
                    e_fwin_zone_new(zone, e_mod_fileman_path_find(zone));
               }
          }
     }
   zone_add_handler = ecore_event_handler_add(E_EVENT_ZONE_ADD,
                                              _e_mod_zone_add, NULL);

   /* FIXME: add system event for new zone creation, and on creation, add an fwin to the zone */

   e_fileman_dbus_init();

   e_fwin_nav_init();
   
   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
   Eina_List *l, *ll, *lll;
   E_Manager *man;
   E_Container *con;
   E_Zone *zone;
   E_Config_Dialog *cfd;

   e_fileman_dbus_shutdown();

   ecore_event_handler_del(zone_add_handler);
   zone_add_handler = NULL;

   /* Unhook zone fm */
   for (l = e_manager_list(); l; l = l->next)
     {
        man = l->data;
        for (ll = man->containers; ll; ll = ll->next)
          {
             con = ll->data;
             for (lll = con->zones; lll; lll = lll->next)
               {
                  zone = lll->data;
                  if (!zone) continue;
                  e_fwin_zone_shutdown(zone);
               }
          }
     }

   e_fwin_nav_shutdown();
   
   /* remove module-supplied menu additions */
   if (maug)
     {
        e_int_menus_menu_augmentation_del("main/1", maug);
        maug = NULL;
     }
   if (mcb)
     {
        e_menu_category_callback_del(mcb);
        mcb = NULL;
     }
   /* remove module-supplied action */
   if (act)
     {
        e_action_predef_name_del(_("Launch"), _("File Manager"));
        e_action_del("fileman");
        act = NULL;
     }
   while ((cfd = e_config_dialog_get("E", "fileman/mime_edit_dialog")))
      e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "fileman/file_icons")))
      e_object_del(E_OBJECT(cfd));
   while ((cfd = e_config_dialog_get("E", "fileman/fileman")))
      e_object_del(E_OBJECT(cfd));
   
   e_configure_registry_item_del("fileman/file_icons");
   e_configure_registry_item_del("fileman/fileman");
   e_configure_registry_category_del("fileman");

   e_config_domain_save("module.fileman", conf_edd, fileman_config);
   _e_mod_fileman_config_free();
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(paths_edd);

   //eina_shutdown();

   conf_module = NULL;
   return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
   e_config_domain_save("module.fileman", conf_edd, fileman_config);
   return 1;
}

/* action callback */
static void
_e_mod_action_fileman_cb(E_Object   *obj,
                         const char *params)
{
   E_Zone *zone = NULL;

   if (obj)
     {
        if (obj->type == E_MANAGER_TYPE)
          zone = e_util_zone_current_get((E_Manager *)obj);
        else if (obj->type == E_CONTAINER_TYPE)
          zone = e_util_zone_current_get(((E_Container *)obj)->manager);
        else if (obj->type == E_ZONE_TYPE)
          zone = e_util_zone_current_get(((E_Zone *)obj)->container->manager);
        else
          zone = e_util_zone_current_get(e_manager_current_get());
     }
   if (!zone) zone = e_util_zone_current_get(e_manager_current_get());
   if (zone)
     {
        if (params && params[0] == '/')
          e_fwin_new(zone->container, "/", params);
        else if (params && params[0] == '~')
          e_fwin_new(zone->container, "~/", params + 1);
        else if (params && strcmp(params, "(none)")) /* avoid matching paths that no longer exist */
          {
             char *path;
             path = e_util_shell_env_path_eval(params);
             if (path)
               {
                  e_fwin_new(zone->container, path, "/");
                  free(path);
               }
          }
        else
          e_fwin_new(zone->container, "favorites", "/");
     }
}

/* menu item callback(s) */
//~ static int
//~ _e_mod_fileman_defer_cb(void *data)
//~ {
//~ E_Zone *zone;

//~ zone = data;
//~ if (zone) e_fwin_new(zone->container, "favorites", "/");
//~ return 0;
//~ }

//~ static void
//~ _e_mod_fileman_cb(void *data, E_Menu *m, E_Menu_Item *mi)
//~ {
//~ ecore_idle_enterer_add(_e_mod_fileman_defer_cb, m->zone);
//~ }

static void
_e_mod_menu_gtk_cb(void           *data,
                   E_Menu         *m,
                   E_Menu_Item *mi __UNUSED__)
{
   Evas_Object *fm;

   m = _e_mod_menu_top_get(m);
   fm = e_object_data_get(E_OBJECT(m));
   if (fm && ((fileman_config->view.open_dirs_in_place && evas_object_data_get(fm, "page_is_window")) ||
       (fileman_config->view.desktop_navigation && evas_object_data_get(fm, "page_is_zone"))))
     e_fm2_path_set(fm, NULL, data);
   else if (m->zone) e_fwin_new(m->zone->container, NULL, data);
}

static void
_e_mod_menu_virtual_cb(void           *data,
                       E_Menu         *m,
                       E_Menu_Item *mi __UNUSED__)
{
   Evas_Object *fm;

   m = _e_mod_menu_top_get(m);
   fm = e_object_data_get(E_OBJECT(m));
   if (fm && ((fileman_config->view.open_dirs_in_place && evas_object_data_get(fm, "page_is_window")) ||
       (fileman_config->view.desktop_navigation && evas_object_data_get(fm, "page_is_zone"))))
     e_fm2_path_set(fm, data, "/");
   else if (m->zone) e_fwin_new(m->zone->container, data, "/");
}

static void
_e_mod_menu_volume_cb(void           *data,
                      E_Menu         *m,
                      E_Menu_Item *mi __UNUSED__)
{
   E_Volume *vol = data;
   Evas_Object *fm;

   m = _e_mod_menu_top_get(m);
   fm = e_object_data_get(E_OBJECT(m));
   if (vol->mounted)
     {
       if (fm && ((fileman_config->view.open_dirs_in_place && evas_object_data_get(fm, "page_is_window")) ||
           (fileman_config->view.desktop_navigation && evas_object_data_get(fm, "page_is_zone"))))
         e_fm2_path_set(fm, NULL, vol->mount_point);
        else if (m->zone)
          e_fwin_new(m->zone->container, NULL, vol->mount_point);
     }
   else
     {
        char buf[PATH_MAX + sizeof("removable:")];

        snprintf(buf, sizeof(buf), "removable:%s", vol->udi);
        if (fm && ((fileman_config->view.open_dirs_in_place && evas_object_data_get(fm, "page_is_window")) ||
            (fileman_config->view.desktop_navigation && evas_object_data_get(fm, "page_is_zone"))))
          e_fm2_path_set(fm, buf, "/");
        else if (m->zone)
          e_fwin_new(m->zone->container, buf, "/");
     }
}

static void
_e_mod_fileman_parse_gtk_bookmarks(E_Menu   *m,
                                   Eina_Bool need_separator)
{
   char line[4096];
   char buf[PATH_MAX];
   E_Menu_Item *mi;
   Efreet_Uri *uri;
   char *alias;
   FILE *fp;

   snprintf(buf, sizeof(buf), "%s/.gtk-bookmarks", e_user_homedir_get());
   fp = fopen(buf, "r");
   if (fp)
     {
        while(fgets(line, sizeof(line), fp))
          {
             alias = NULL;
             line[strlen(line) - 1] = '\0';
             alias = strchr(line, ' ');
             if (alias)
               {
                  line[alias - line] = '\0';
                  alias++;
               }
             uri = efreet_uri_decode(line);
             if (uri && uri->path)
               {
                  if (ecore_file_exists(uri->path))
                    {
                       if (need_separator)
                         {
                            mi = e_menu_item_new(m);
                            e_menu_item_separator_set(mi, 1);
                            need_separator = 0;
                         }

                       mi = e_menu_item_new(m);
                       e_object_data_set(E_OBJECT(mi), uri->path);
                       e_menu_item_label_set(mi, alias ? alias :
                                             ecore_file_file_get(uri->path));
                       e_util_menu_item_theme_icon_set(mi, "folder");
                       e_menu_item_callback_set(mi, _e_mod_menu_gtk_cb,
                                                (void *)eina_stringshare_add(uri->path));
                       e_menu_item_submenu_pre_callback_set(mi, _e_mod_menu_populate, eina_stringshare_add("/"));
                    }
               }
             if (uri) efreet_uri_free(uri);
          }
        fclose(fp);
     }
}

static E_Menu *
_e_mod_menu_top_get(E_Menu *m)
{
   while (m->parent_item)
     {
        if (m->parent_item->menu->header.title)
          break;
        m = m->parent_item->menu;
     }
   return m;
}

static void
_e_mod_menu_populate_cb(void      *data,
                       E_Menu      *m,
                       E_Menu_Item *mi)
{
   const char *path;
   Evas_Object *fm;

   if (!m->zone) return;
   m = _e_mod_menu_top_get(m);

   fm = e_object_data_get(E_OBJECT(m));
   path = e_object_data_get(E_OBJECT(mi));
   if (fm && ((fileman_config->view.open_dirs_in_place && evas_object_data_get(fm, "page_is_window")) ||
       (fileman_config->view.desktop_navigation && evas_object_data_get(fm, "page_is_zone"))))
     e_fm2_path_set(fm, data, path ?: "/");
   else if (m->zone)
     e_fwin_new(m->zone->container, data, path ?: "/");
}

static void
_e_mod_menu_cleanup_cb(void *obj)
{
   eina_stringshare_del(e_object_data_get(E_OBJECT(obj)));
}

static Eina_Bool
_e_mod_menu_populate_filter(void *data __UNUSED__, Eio_File *handler __UNUSED__, const Eina_File_Direct_Info *info)
{
   /* don't show .dotfiles */
   if (fileman_config->view.menu_shows_files)
     return (info->path[info->name_start] != '.');
   return (info->path[info->name_start] != '.') && (info->type == EINA_FILE_DIR);
}

static void
_e_mod_menu_populate_item(void *data, Eio_File *handler __UNUSED__, const Eina_File_Direct_Info *info)
{
   E_Menu *m = data;
   E_Menu_Item *mi;
   const char *dev, *path;

   mi = m->parent_item;
   dev = e_object_data_get(E_OBJECT(m));
   path = mi ? e_object_data_get(E_OBJECT(mi)) : "/";
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, info->path + info->name_start);
   if (fileman_config->view.menu_shows_files)
     {
        if (info->type != EINA_FILE_DIR)
          {
             const char *mime = NULL;
             Efreet_Desktop *ed = NULL;
             char group[1024];

             if (eina_str_has_extension(mi->label, "desktop"))
               {
                  ed = efreet_desktop_new(info->path);
                  if (ed)
                    {
                       e_util_menu_item_theme_icon_set(mi, ed->icon);
                       efreet_desktop_free(ed);
                       return;
                    }
               }
             mime = efreet_mime_type_get(mi->label);
             if (!mime) return;
             if (!strncmp(mime, "image/", 6))
               {
                  e_menu_item_icon_file_set(mi, info->path);
                  return;
               }
             snprintf(group, sizeof(group), "fileman/mime/%s", mime);
             if (e_util_menu_item_theme_icon_set(mi, group))
               return;
             e_util_menu_item_theme_icon_set(mi, "fileman/mime/unknown");
             return;
          }
     }
   e_util_menu_item_theme_icon_set(mi, "folder");
   e_object_data_set(E_OBJECT(mi), eina_stringshare_printf("%s/%s", path ?: "/", info->path + info->name_start));
   //fprintf(stderr, "PATH SET: %s\n", e_object_data_get(E_OBJECT(mi)));
   e_object_free_attach_func_set(E_OBJECT(mi), _e_mod_menu_cleanup_cb);
   e_menu_item_callback_set(mi, _e_mod_menu_populate_cb, dev);
   e_menu_item_submenu_pre_callback_set(mi, _e_mod_menu_populate, eina_stringshare_ref(dev));
}

static void
_e_mod_menu_populate_err(void *data, Eio_File *handler __UNUSED__, int error __UNUSED__)
{
   if (!e_object_unref(data)) return;
   e_menu_thaw(data);
}

static int
_e_mod_menu_populate_sort(E_Menu_Item *a, E_Menu_Item *b)
{
   return strcmp(a->label, b->label);
}

static void
_e_mod_menu_populate_done(void *data, Eio_File *handler __UNUSED__)
{
   E_Menu *m = data;
   if (!e_object_unref(data)) return;
   if (!m->items)
     {
        e_menu_deactivate(m);
        if (m->parent_item)
          e_menu_item_submenu_set(m->parent_item, NULL);
        return;
     }
   m->items = eina_list_sort(m->items, 0, (Eina_Compare_Cb)_e_mod_menu_populate_sort);
   e_menu_thaw(m);
}

static void
_e_mod_menu_populate(void *d, E_Menu *m __UNUSED__, E_Menu_Item *mi)
{
   E_Menu *subm;
   const char *dev, *path, *rp;
   Eio_File *ls;

   subm = mi->submenu;
   if (subm && subm->items) return;
   if (!subm)
     {
        subm = e_menu_new();
        e_object_data_set(E_OBJECT(subm), d);
        e_object_free_attach_func_set(E_OBJECT(subm), _e_mod_menu_cleanup_cb);
        e_menu_item_submenu_set(mi, subm);
        e_menu_freeze(subm);
     }
   dev = d;
   path = mi ? e_object_data_get(E_OBJECT(mi)) : NULL;
   rp = e_fm2_real_path_map(dev, path ?: "/");
   ls = eio_file_stat_ls(rp, _e_mod_menu_populate_filter, _e_mod_menu_populate_item, _e_mod_menu_populate_done, _e_mod_menu_populate_err, subm);
   EINA_SAFETY_ON_NULL_RETURN(ls);
   e_object_ref(E_OBJECT(subm));
   eina_stringshare_del(rp);
}

static void
_e_mod_menu_free(void *data)
{
   Eina_List *l;
   E_Menu_Item *mi;
   E_Menu *m = data;

   EINA_LIST_FOREACH(m->items, l, mi)
     if (mi->submenu)
       {
          //INF("SUBMENU %p REF: %d", mi->submenu, e_object_ref_get(E_OBJECT(mi->submenu)) - 1);
          _e_mod_menu_free(mi->submenu);
          e_object_unref(E_OBJECT(mi->submenu));
       }
}

/* menu item add hook */
static void
_e_mod_menu_generate(void *data __UNUSED__, E_Menu *m)
{
   E_Volume *vol;
   E_Menu_Item *mi;
   const char *s;
   const Eina_List *l;
   Eina_Bool need_separator;
   Eina_Bool volumes_visible = 0;

   if (m->items) return;
   e_object_free_attach_func_set(E_OBJECT(m), _e_mod_menu_free);

   /* Home */
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Home"));
   e_util_menu_item_theme_icon_set(mi, "user-home");
   s = eina_stringshare_add("~/");
   e_menu_item_callback_set(mi, _e_mod_menu_virtual_cb, s);
   e_menu_item_submenu_pre_callback_set(mi, _e_mod_menu_populate, s);

   /* Desktop */
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Desktop"));
   e_util_menu_item_theme_icon_set(mi, "user-desktop");
   s = eina_stringshare_add("desktop");
   e_menu_item_callback_set(mi, _e_mod_menu_virtual_cb, s);
   e_menu_item_submenu_pre_callback_set(mi, _e_mod_menu_populate, s);

   /* Favorites */
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Favorites"));
   e_util_menu_item_theme_icon_set(mi, "user-bookmarks");
   s = eina_stringshare_add("favorites");
   e_menu_item_callback_set(mi, _e_mod_menu_virtual_cb, s);
   e_menu_item_submenu_pre_callback_set(mi, _e_mod_menu_populate, s);

   /* Trash */
   //~ mi = e_menu_item_new(em);
   //~ e_menu_item_label_set(mi, D_("Trash"));
   //~ e_util_menu_item_theme_icon_set(mi, "user-trash");
   //~ e_menu_item_callback_set(mi, _places_run_fm, "trash:///");

   /* Root */
   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Root"));
   e_util_menu_item_theme_icon_set(mi, "computer");
   s = eina_stringshare_add("/");
   e_menu_item_callback_set(mi, _e_mod_menu_virtual_cb, s);
   e_menu_item_submenu_pre_callback_set(mi, _e_mod_menu_populate, s);
   need_separator = 1;

   /* Volumes */
   EINA_LIST_FOREACH(e_fm2_device_volume_list_get(), l, vol)
     {
        if (vol->mount_point && !strcmp(vol->mount_point, "/")) continue;

        if (need_separator)
          {
             mi = e_menu_item_new(m);
             e_menu_item_separator_set(mi, 1);
             need_separator = 0;
          }

        mi = e_menu_item_new(m);
        e_menu_item_label_set(mi, vol->label);
        e_util_menu_item_theme_icon_set(mi, vol->icon);
        e_menu_item_callback_set(mi, _e_mod_menu_volume_cb, vol);
        volumes_visible = 1;
     }

   /* Favorites */
   //~ if (places_conf->show_bookm)
   //~ {
   _e_mod_fileman_parse_gtk_bookmarks(m, need_separator || volumes_visible > 0);
   //~ }

   e_menu_pre_activate_callback_set(m, NULL, NULL);
}

void
_e_mod_menu_add(void *data __UNUSED__,
                E_Menu    *m)
{
#ifdef ENABLE_FILES
   E_Menu_Item *mi;
   E_Menu *sub;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, _("Navigate..."));
   e_util_menu_item_theme_icon_set(mi, "system-file-manager");
   sub = e_menu_new();
   e_menu_item_submenu_set(mi, sub);
   e_object_unref(E_OBJECT(sub)); //allow deletion whenever main menu deletes
   e_menu_pre_activate_callback_set(sub, _e_mod_menu_generate, NULL);
#else
   (void)m;
#endif
}

static void
_e_mod_main_menu_cb(E_Menu *m, void *category_data __UNUSED__, void *data __UNUSED__)
{
#ifdef ENABLE_FILES
   E_Menu_Item *mi;
   _e_mod_menu_add(NULL, m);
   m->items = eina_list_promote_list(m->items, eina_list_last(m->items));
   mi = e_menu_item_new_relative(m, m->items->data);
   e_menu_item_separator_set(mi, EINA_TRUE);
#else
   (void)m;
#endif
}

/* Abstract fileman config load/create to one function for maintainability */
static void
_e_mod_fileman_config_load(void)
{
#undef T
#undef D
#define T Fileman_Path
#define D paths_edd
   paths_edd = E_CONFIG_DD_NEW("Fileman_Path", Fileman_Path);
   E_CONFIG_VAL(D, T, dev, STR);
   E_CONFIG_VAL(D, T, path, STR);
   E_CONFIG_VAL(D, T, zone, UINT);
   E_CONFIG_VAL(D, T, desktop_mode, INT);
   conf_edd = E_CONFIG_DD_NEW("Fileman_Config", Config);
   #undef T
   #undef D
   #define T Config
   #define D conf_edd
   E_CONFIG_VAL(D, T, config_version, INT);
   E_CONFIG_VAL(D, T, view.mode, INT);
   E_CONFIG_VAL(D, T, view.open_dirs_in_place, UCHAR);
   E_CONFIG_VAL(D, T, view.selector, UCHAR);
   E_CONFIG_VAL(D, T, view.single_click, UCHAR);
   E_CONFIG_VAL(D, T, view.no_subdir_jump, UCHAR);
   E_CONFIG_VAL(D, T, view.no_subdir_drop, UCHAR);
   E_CONFIG_VAL(D, T, view.always_order, UCHAR);
   E_CONFIG_VAL(D, T, view.link_drop, UCHAR);
   E_CONFIG_VAL(D, T, view.fit_custom_pos, UCHAR);
   E_CONFIG_VAL(D, T, view.show_full_path, UCHAR);
   E_CONFIG_VAL(D, T, view.show_desktop_icons, UCHAR);
   E_CONFIG_VAL(D, T, view.show_toolbar, UCHAR);
   E_CONFIG_VAL(D, T, view.show_sidebar, UCHAR);
   E_CONFIG_VAL(D, T, view.desktop_navigation, UCHAR);
   E_CONFIG_VAL(D, T, icon.icon.w, INT);
   E_CONFIG_VAL(D, T, icon.icon.h, INT);
   E_CONFIG_VAL(D, T, icon.list.w, INT);
   E_CONFIG_VAL(D, T, icon.list.h, INT);
   E_CONFIG_VAL(D, T, icon.fixed.w, UCHAR);
   E_CONFIG_VAL(D, T, icon.fixed.h, UCHAR);
   E_CONFIG_VAL(D, T, icon.extension.show, UCHAR);
   E_CONFIG_VAL(D, T, list.sort.no_case, UCHAR);
   E_CONFIG_VAL(D, T, list.sort.extension, UCHAR);
   E_CONFIG_VAL(D, T, list.sort.mtime, UCHAR);
   E_CONFIG_VAL(D, T, list.sort.size, UCHAR);
   E_CONFIG_VAL(D, T, list.sort.dirs.first, UCHAR);
   E_CONFIG_VAL(D, T, list.sort.dirs.last, UCHAR);
   E_CONFIG_VAL(D, T, selection.single, UCHAR);
   E_CONFIG_VAL(D, T, selection.windows_modifiers, UCHAR);
   E_CONFIG_VAL(D, T, theme.background, STR);
   E_CONFIG_VAL(D, T, theme.frame, STR);
   E_CONFIG_VAL(D, T, theme.icons, STR);
   E_CONFIG_VAL(D, T, theme.fixed, UCHAR);
   E_CONFIG_VAL(D, T, tooltip.delay, DOUBLE);
   E_CONFIG_VAL(D, T, tooltip.size, DOUBLE);
   E_CONFIG_VAL(D, T, tooltip.enable, UCHAR);
   E_CONFIG_LIST(D, T, paths, paths_edd);

   fileman_config = e_config_domain_load("module.fileman", conf_edd);
   if (fileman_config)
     {
        if (!e_util_module_config_check("Fileman", fileman_config->config_version, MOD_CONFIG_FILE_VERSION))
          _e_mod_fileman_config_free();
     }

   if (!fileman_config)
     {
        fileman_config = E_NEW(Config, 1);
        fileman_config->config_version = (MOD_CONFIG_FILE_EPOCH << 16);
     }
#define IFMODCFG(v) \
  if ((fileman_config->config_version & 0xffff) < (v)) {
#define IFMODCFGEND }

    IFMODCFG(0x008d);
    fileman_config->view.mode = E_FM2_VIEW_MODE_GRID_ICONS;
    fileman_config->view.open_dirs_in_place = 0;
    fileman_config->view.selector = 0;
    fileman_config->view.single_click = 0;
    fileman_config->view.no_subdir_jump = 0;
    fileman_config->view.show_full_path = 0;
    fileman_config->view.show_desktop_icons = 1;
    fileman_config->icon.icon.w = 48;
    fileman_config->icon.icon.h = 48;
    fileman_config->icon.fixed.w = 0;
    fileman_config->icon.fixed.h = 0;
    fileman_config->icon.extension.show = 1;
    fileman_config->list.sort.no_case = 1;
    fileman_config->list.sort.dirs.first = 1;
    fileman_config->list.sort.dirs.last = 0;
    fileman_config->selection.single = 0;
    fileman_config->selection.windows_modifiers = 0;
    IFMODCFGEND;

    IFMODCFG(0x0101);
    fileman_config->view.show_toolbar = 1;
    fileman_config->view.open_dirs_in_place = 1;
    IFMODCFGEND;

    IFMODCFG(0x0104);
    fileman_config->tooltip.delay = 1.0;
    fileman_config->tooltip.size = 30.0;
    IFMODCFGEND;

    IFMODCFG(0x0105);
    e_config->filemanager_single_click = fileman_config->view.single_click;
    IFMODCFGEND;

    IFMODCFG(0x0107);
    fileman_config->view.show_sidebar = 1;
    IFMODCFGEND;

    IFMODCFG(0x0108);
    fileman_config->view.menu_shows_files = 0;
    IFMODCFGEND;

    IFMODCFG(0x0109);
    fileman_config->view.desktop_navigation = 0;
    IFMODCFGEND;

    IFMODCFG(0x0110);
    fileman_config->tooltip.enable = 1;
    IFMODCFGEND;

    fileman_config->config_version = MOD_CONFIG_FILE_VERSION;

    /* UCHAR's give nasty compile warnings about comparisons so not gonna limit those */
    E_CONFIG_LIMIT(fileman_config->view.mode, E_FM2_VIEW_MODE_ICONS, E_FM2_VIEW_MODE_LIST);
    E_CONFIG_LIMIT(fileman_config->icon.icon.w, 16, 256);
    E_CONFIG_LIMIT(fileman_config->icon.icon.h, 16, 256);
    E_CONFIG_LIMIT(fileman_config->icon.list.w, 16, 256);
    E_CONFIG_LIMIT(fileman_config->icon.list.h, 16, 256);

    E_CONFIG_LIMIT(fileman_config->tooltip.delay, 0.0, 5.0);
    E_CONFIG_LIMIT(fileman_config->tooltip.size, 10.0, 75.0);

    e_config_save_queue();
}

static void
_e_mod_fileman_path_free(Fileman_Path *path)
{
   if (!path) return;
   eina_stringshare_del(path->dev);
   eina_stringshare_del(path->path);
   free(path);
}

static void
_e_mod_fileman_config_free(void)
{
   eina_stringshare_del(fileman_config->theme.background);
   eina_stringshare_del(fileman_config->theme.frame);
   eina_stringshare_del(fileman_config->theme.icons);
   E_FREE_LIST(fileman_config->paths, _e_mod_fileman_path_free);
   E_FREE(fileman_config);
}

static Eina_Bool
_e_mod_zone_add(__UNUSED__ void *data,
                int              type,
                void            *event)
{
   E_Event_Zone_Add *ev;
   E_Zone *zone;

   if (type != E_EVENT_ZONE_ADD) return ECORE_CALLBACK_PASS_ON;
   ev = event;
   zone = ev->zone;
   if (e_fwin_zone_find(zone)) return ECORE_CALLBACK_PASS_ON;
   if (fileman_config->view.show_desktop_icons)
     e_fwin_zone_new(zone, e_mod_fileman_path_find(zone));
   return ECORE_CALLBACK_PASS_ON;
}

Fileman_Path *
e_mod_fileman_path_find(E_Zone *zone)
{
   Eina_List *l;
   Fileman_Path *path;
   char buf[256];

   EINA_LIST_FOREACH(fileman_config->paths, l, path)
     if (path->zone == zone->container->num + zone->num) return path;
   path = E_NEW(Fileman_Path, 1);
   path->zone = zone->container->num + zone->num;
   path->dev = eina_stringshare_add("desktop");
   path->desktop_mode = E_FM2_VIEW_MODE_CUSTOM_ICONS;
   if ((zone->container->num == 0) && (zone->num == 0))
     path->path = eina_stringshare_add("/");
   else
     {
        snprintf(buf, sizeof(buf), "%i", (zone->container->num + zone->num));
        path->path = eina_stringshare_add(buf);
     }
   fileman_config->paths = eina_list_append(fileman_config->paths, path);
   return path;
}
