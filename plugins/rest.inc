<?php

/**
 * @file
 * Callbacks for a pushing to Mongo through REST API.
 */

/**
 * Call a REST service method.
 *
 * @param  string $method
 * @param  array  $body
 * @return mixed  response object or NULL.
 */
function mobilesearch_rest_service_call($method, $body = array()) {
  $service_url = variable_get('mobilesearch_rest_url', '');
  $agency_id = variable_get('mobilesearch_rest_agency_id', '100000');
  $key = variable_get('mobilesearch_rest_key', NULL);
  $debug = variable_get('mobilesearch_rest_debug', FALSE);
  if (empty($service_url) || empty($agency_id) || empty($key)) {
    watchdog('rest_plugin', "Service doesn't configured. Please !configure_url it first.", array('!configure_url' => l(t('configure'), 'admin/config/mobilesearch/content-export/plugin')), WATCHDOG_ERROR);
    return NULL;
  }
  $instance = new \NationalStorageComponent\NationalStorage($service_url);

  $request = array(
    'credentials' => array(
      'agencyId' => $agency_id,
      'key' => sha1($agency_id . $key),
    ),
    'body' => $body,
  );
  if (!method_exists($instance, $method)) {
    watchdog('rest_plugin', 'You tried to call a undefined method %method of a service.', array('%method' => $method), WATCHDOG_ERROR);
    return NULL;
  }
  try {
    $instance->$method($request);
    $response = $instance->getResponse();
  }
  catch (Exception $e) {
    watchdog_exception('rest_service', $e);
  }

  if ($debug) {
    watchdog('rest_plugin', 'Method: <strong>!method</strong>. Request: <pre>!request</pre>Response: <pre>!response</pre>', array('!method' => $method, '!request' => var_export($request, TRUE), '!response' => var_export($response, TRUE)), WATCHDOG_DEBUG);
  }

  return $response;
}

/**
 * Returns differences agency ids between changed and unchanged nodes.
 *
 * @param  object $node     Changed node.
 * @param  mixed  $original Unchanged node or NULL.
 * @return array
 *  An associative array with the following contents:
 *  <code>
 *  [
 *    to_add: [],
 *    to_update: [],
 *    to_delete: []
 *  ]
 *  </code>
 */
function mobilesearch_rest_node_agency_diff($node, $original = NULL, $op = NULL) {
  $result = array(
    'to_add' => array(),
    'to_update' => array(),
    'to_delete' => array(),
  );

  $agency_id = variable_get('mobilesearch_rest_agency_id', '100000');
  if ($agency_id && $op) {
    switch ($op) {
      case 'hook_node_insert':
        $result['to_add'][] = $agency_id;
        return $result;
      case 'hook_node_update':
        $result['to_update'][] = $agency_id;
        return $result;
      case 'hook_node_delete':
        $result['to_delete'][] = $agency_id;
        return $result;
    }
  }

  $node_realm = array();
  if (!empty($node->field_realm[LANGUAGE_NONE])) {
    foreach ($node->field_realm[LANGUAGE_NONE] as $row) {
      $term = taxonomy_term_load($row['tid']);
      if (!empty($term->field_agency_id[LANGUAGE_NONE][0]['value'])) {
        $agency_id = $term->field_agency_id[LANGUAGE_NONE][0]['value'];
        $node_realm[] = $agency_id;
      }
    }
  }
  $orig_realm = array();
  if (is_object($original)) {
    if (!empty($original->field_realm[LANGUAGE_NONE])) {
      foreach ($original->field_realm[LANGUAGE_NONE] as $row) {
        $term = taxonomy_term_load($row['tid']);
        if (!empty($term->field_agency_id[LANGUAGE_NONE][0]['value'])) {
          $agency_id = $term->field_agency_id[LANGUAGE_NONE][0]['value'];
          $orig_realm[] = $agency_id;
        }
      }
    }
  }

  $result['to_add'] = array_diff($node_realm, $orig_realm);
  $result['to_update'] = array_diff($node_realm, $result['to_add']);
  $result['to_delete'] = array_diff($orig_realm, $node_realm);

  return $result;
}

/**
 * Config form callback for a REST plugin.
 */
function mobilesearch_rest_config_form($form, &$form_state) {
  $form['mobilesearch_rest_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Service URL'),
    '#default_value' => variable_get('mobilesearch_rest_url', ''),
    '#description' => t('URL to service such as "http://example.com:1337".'),
    '#required' => TRUE,
  );
  $form['mobilesearch_rest_agency_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Agency ID'),
    '#default_value' => variable_get('mobilesearch_rest_agency_id', '100000'),
    '#required' => TRUE,
  );
  $form['mobilesearch_rest_key'] = array(
    '#type' => 'textfield',
    '#title' => t('Key'),
    '#default_value' => variable_get('mobilesearch_rest_key', NULL),
    '#required' => TRUE,
  );
  $form['mobilesearch_rest_node_form_button'] = array(
    '#type' => 'checkbox',
    '#title' => t('Node form button'),
    '#default_value' => variable_get('mobilesearch_rest_node_form_button', TRUE),
    '#description' => t('Display "Save and push to Mongo" button in a node editing form.'),
  );
  $form['mobilesearch_rest_debug'] = array(
    '#type' => 'checkbox',
    '#title' => t('Debug'),
    '#default_value' => variable_get('mobilesearch_rest_debug', FALSE),
    '#description' => t('Store debug data in watchdog.'),
  );

  return $form;
}

/**
 * Node editing form alter callback.
 */
function mobilesearch_rest_node_form(&$form, &$form_state) {
  $types = _ding_mobilesearch_types();
  if (variable_get('mobilesearch_rest_node_form_button', TRUE) && in_array($form['type']['#value'], array_keys($types), TRUE)) {
    $form['actions']['submit_rest'] = array(
      '#type' => 'submit',
      '#value' => t('Save and push to Mongo'),
      '#submit' => array(
        'node_form_submit',
        'mobilesearch_rest_node_submit',
      ),
      '#weight' => 6,
    );
  }
}

/**
 * Submit handler for a node editing form.
 */
function mobilesearch_rest_node_submit($form, &$form_state) {
  $node = $form_state['node'];
  $types = _ding_mobilesearch_types();
  $plugin = $types[$node->type];
  if (!empty($plugin['exec_callback']) && function_exists($plugin['exec_callback'])) {
    $plugin['exec_callback']('hook_node_update', 'node', $node);
  }
  $form_state['redirect'] = 'node/' . $node->nid;
}

/**
 * Plugin execute callback.
 */
function mobilesearch_rest_exec($op, $entity_type, $entity) {
  // Export menu links.
  if ($entity_type == 'menu_link') {
    $method_mapping = array(
      'hook_menu_link_insert' => 'addMenu',
      'hook_menu_link_update' => 'updateMenu',
      'hook_menu_link_delete' => 'deleteMenu',
    );

    if (!preg_match('/(main|left)/', $entity['menu_name'], $matches)) {
      return;
    }
    $type = $matches[1] . '_menu';

    $agency_id = variable_get('mobilesearch_menu_agency_id__' . $entity['menu_name'], 0);
    $request_body = array(
      'mlid' => $entity['mlid'],
      'agency' => $agency_id,
      'type' => $type,
      'name' => $entity['link_title'],
      'url' => '/' . $entity['link_path'],
      'order' => $entity['weight'],
    );
    mobilesearch_rest_service_call($method_mapping[$op], $request_body);
  }
  // Export content through node hooks.
  elseif ($entity_type == 'node') {
    switch ($op) {
      case 'hook_node_insert':
        $diff = mobilesearch_rest_node_agency_diff($entity, NULL, $op);
        break;
      case 'hook_node_update':
        $original = isset($entity->original) ? $entity->original : NULL;
        $diff = mobilesearch_rest_node_agency_diff($entity, $original, $op);
        break;
      case 'hook_node_delete':
        $original = clone $entity;
        $entity->field_realm = array();
        $diff = mobilesearch_rest_node_agency_diff($entity, $original, $op);
        break;
    }

    $mapping = module_invoke_all('mobilesearch_node_export_mapping', $entity);
    $request_body = array(
      'nid' => $entity->nid,
      'agency' => NULL,
      'type' => $entity->type,
    );
    if (isset($mapping['special']) && is_array($mapping['special'])) {
      $request_body = array_merge($request_body, $mapping['special']);
    }
    $request_body['list'] = 0;
    $request_body['fields'] = $mapping['fields'];
    $request_body['taxonomy'] = $mapping['taxonomy'];

    foreach ($diff['to_add'] as $agency_id) {
      $request_body['agency'] = $agency_id;
      mobilesearch_rest_service_call('addContent', $request_body);
    }
    foreach ($diff['to_update'] as $agency_id) {
      $request_body['agency'] = $agency_id;
      $response = mobilesearch_rest_service_call('addContent', $request_body);
      if (!$response || (is_object($response['response']) && $response['response']->status == FALSE)) {
        mobilesearch_rest_service_call('updateContent', $request_body);
      }
    }
    foreach ($diff['to_delete'] as $agency_id) {
      $request_body['agency'] = $agency_id;
      mobilesearch_rest_service_call('deleteContent', $request_body);
    }
  }
  // Export content through workflow hook.
  elseif ($entity_type == 'workflow') {
    $os_nid = $entity->field_os_node[LANGUAGE_NONE][0]['nid'];
    if ($os_nid) {
      $os_node = node_load($os_nid);
      $mapping = module_invoke_all('mobilesearch_node_export_mapping', $entity);
      $diff = mobilesearch_rest_node_agency_diff($os_node);
      foreach ($diff['to_add'] as $agency_id) {
        $request_body = array(
          'nid' => $os_node->nid,
          'agency' => $agency_id,
          'type' => $os_node->type,
        );
        if (isset($mapping['special']) && is_array($mapping['special'])) {
          $request_body = array_merge($request_body, $mapping['special']);
        }
        $request_body['list'] = 0;
        $request_body['fields'] = $mapping['fields'];
        $request_body['taxonomy'] = $mapping['taxonomy'];

        mobilesearch_rest_service_call('addContent', $request_body);
      }
    }
  }
}
