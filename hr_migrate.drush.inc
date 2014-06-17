<?php

/**
 * @file
 * hr_migrate module drush integration.
 */

/**
 * Implements hook_drush_command().
 *
 * @return
 *   An associative array describing your command(s).
 *
 * @see drush_parse_command()
 */
function hr_migrate_drush_command() {
  $items = array();

  $items['hr-migrate-job-titles'] = array(
    'description' => "Migrate the job titles to a text field",
    'options' => array(
    ),
    'drupal dependencies' => array('hr_users'),
    'aliases' => array(),
  );

  return $items;
}

function drush_hr_migrate_job_titles() {
  // Load all profiles
  $query = new EntityFieldQuery();
  $profiles = $query->entityCondition('entity_type', 'profile2')
    ->execute();
  if (isset($profiles['profile2'])) {
    $pids = array_keys($profiles['profile2']);
    foreach ($pids as $pid) {
      $profile = profile2_load($pid);
      if (!isset($profile->field_job_title_other[LANGUAGE_NONE][0]['value'])) {
        $tid = $profile->field_job_title[LANGUAGE_NONE][0]['target_id'];
        $term = taxonomy_term_load($tid);
        $profile->field_job_title_other[LANGUAGE_NONE][0]['value'] = $term->name;
        profile2_save($profile);
      }
    }
  }

  // Load all contacts
  $query = new EntityFieldQuery();
  $contacts = $query->entityCondition('entity_type', 'node')
    ->entityCondition('bundle', 'hr_contact')
    ->execute();
  if (isset($contacts['node'])) {
    $nids = array_keys($contacts['node']);
    foreach ($nids as $nid) {
      $contact = node_load($nid);
      if (!isset($contact->field_job_title_other[LANGUAGE_NONE][0]['value'])) {
        $tid = $contact->field_job_title[LANGUAGE_NONE][0]['target_id'];
        $term = taxonomy_term_load($tid);
        $contact->field_job_title_other[LANGUAGE_NONE][0]['value'] = $term->name;
        node_save($contact);
      }
    }
  }

}
