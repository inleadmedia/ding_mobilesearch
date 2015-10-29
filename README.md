# Ding Mobilesearch
Mobilesearch provides a export to Mongo functionality for Ding2 project, but
can be used in another projects. Basically provides export through the REST API
but can be expanded with a plugins (see hook_mobilesearch_plugin_info()).

# Installation
Is very simple. Just enable the module and then configure it.
Also supports (and provides a necessary features) workflow and workbench modules
in the case if they are enabled. This isn't a necessary requirements therefore don't
includes in a dependencies section.

## Dependencies
* Drupal 7.x.

## Configuration.
This module implements a hook_ding_install_tasks() and can be configured during install.
Also you can skip these steps when install and you can set up module later. Just
follow these steps:
* Go to admin/config/ding/mobilesearch/content-export and specify the node types
  and menus which should be exported.
* Then go to admin/config/ding/mobilesearch/content-export/plugin and set the configs for
  all known plugins. Notice that "Agency ID" setting is the same as
  "Library code" (admin/config/ting/settings).

# Post installation

## Workbench integration
As we know Workbench works through Views module. Therefore integration with Workbench
works through VBO (views_bulk_operations). Our module implements a hook_action_info()
which defines "Push to Mongo" action for VBO. You are free to add new field
"Bulk operations: Content" in a necessary views and enable "Push to Mongo" action in
"SELECTED BULK OPERATIONS" section (when the field is  configured).
I.e. Workbench integration don't works "out of the box" should be configured.
