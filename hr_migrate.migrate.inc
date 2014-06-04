<?php
/**
 * @file
 * Examples and test fodder for migration into profile2 entities.
 */

class HRMigration extends DrupalNode7Migration {

  protected function getLegacyTermName($val) {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('taxonomy_term_data', 't')
             ->fields('t', array('name'))
             ->condition('tid', $val);
    $result = $query->execute()->fetchAssoc();
    $term_name = isset($result['name']) ? $result['name'] : '';
    return $term_name;
  }

  protected function translateTerms($value, $vocabulary) {
    if (!is_array($value)) {
      $value = array($value);
    }
    $return  = array();
    foreach ($value as $i => $val) {
      $term_name = $this->getLegacyTermName($val);
      if (!empty($term_name)) {
        $terms = taxonomy_get_term_by_name($term_name, $vocabulary);
        if (!empty($terms)) {
          $return[$i] = reset(array_keys($terms));
        }
        else {
          debug('Could not find term '.$term_name.' in current database');
        }
      }
      else {
        debug('Could not find term with tid '.$val.' in legacy database');
      }
    }

    if ($this->arguments['source_connection'] == 'assessments' && $vocabulary == 'hr_theme') {
      $terms = taxonomy_get_term_by_name('Needs Assessment', $vocabulary);
      if (!empty($terms)) {
       $keys = array_keys($terms);
        $return[] = reset($keys);
      }
    }
    return $return;
  }

  protected function translateNodes($value, $type, $gid = NULL) {
    if (!is_array($value)) {
      $value = array($value);
    }
    $return  = array();
    foreach ($value as $i => $val) {
      $term_name = $this->getLegacyTermName($val);
      if ($term_name == 'Child Protection') {
        $term_name = 'Protection';
      }
      if (!empty($term_name)) {
        $query = new EntityFieldQuery();
        $query
          ->entityCondition('entity_type', 'node')
          ->entityCondition('bundle', $type)
          ->propertyCondition('title', $term_name);
        if ($gid) {
          $query->fieldCondition('og_group_ref', 'target_id', $gid, '=');
        }
        $entities = $query->execute();
        if (!empty($entities['node'])) {
          if (count($entities['node']) == 1) {
            $return[$i] = reset(array_keys($entities['node']));
          }
          else {
            debug('Multiple nodes with title '.$term_name.' in current database');
          }
        }
        else {
          debug('Could not find node '.$term_name.' in current database');
        }
      }
      else {
        debug('Could not find term with tid '.$val.' in legacy database');
      }
    }
    return $return;
  }

  protected function fieldThemes($value) {
    return $this->translateTerms($value, 'hr_theme');
  }

  protected function fieldDocumentTypes($value) {
    return $this->translateTerms($value, 'hr_document_type');
  }

  protected function fieldMapTypes($value) {
    return $this->translateTerms($value, 'hr_infographic_type');
  }


  protected function fieldOrganizations($value) {
    return $this->translateTerms($value, 'hr_organization');
  }

  protected function fieldSectors($value) {
    return $this->translateNodes($value, 'hr_sector');
  }

  protected function fieldBundles($value) {
    $gid = $this->getSpace();
    return $this->translateNodes($value, 'hr_bundle', $gid);
  }

  protected function fieldLocations($value) {
    return $this->translateTerms($value, 'hr_location');
  }

  protected function fieldFundings($value) {
    return $this->translateTerms($value, 'hr_funding_type');
  }

  protected function fieldFundingMethods($value) {
    $gid = $this->getSpace();
    return $this->translateNodes($value, 'hr_funding_method', $gid);
  }


  protected function fieldJobTitle($value) {
    return $this->translateTerms($value, 'hr_job_title');
  }

  protected function fieldGroupAudience($value) {
    // Expecting value of field_themes
    if (!is_array($value)) {
      $value = array($value);
    }
    if ($this->arguments['source_connection'] == 'www') {
      $valid_spaces = array(
        'Age',
        'Environment',
        'Gender',
        'GenCap',
        'Accountability to Affected Populations',
        'ProCap',
      );
      $return  = array();
      $index = NULL;
      foreach ($value as $i => $val) {
        $term_name = $this->getLegacyTermName($val);
        if (in_array($term_name, $valid_spaces)) {
            $index = $i;
        }
      }
      if ($index !== NULL) {
        return $this->translateNodes($value[$index], 'hr_space');
      }
      else {
        return NULL;
      }
    }
  }

  protected function getSpace() {
    $target_id = NULL;
    $groups = array(
      'clusters' => 'Clusters',
      'imwg' => 'Information Management Working Group',
      'ir' => 'Indicators Registry',
      'er' => 'Early Recovery',
      'assessments' => 'Humanitarian Programme Cycle',
      'cod' => 'Data',
    );
    $type = 'hr_space';
    if ($this->arguments['source_connection'] == 'er') {
      $type = 'hr_sector';
    }
    $title = '';
    if (in_array($this->arguments['source_connection'], array_keys($groups))) {
      $title = $groups[$this->arguments['source_connection']];
    }
    $operations = array(
      'ethiopia' => 'Ethiopia',
    );
    if (in_array($this->arguments['source_connection'], array_keys($operations))) {
      $type = 'hr_operation';
      $title = $operations[$this->arguments['source_connection']];
    }
    $query = new EntityFieldQuery();
    $entities = $query
      ->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $type)
      ->propertyCondition('title', $title)
      ->execute();
    if (!empty($entities['node'])) {
      if (count($entities['node']) == 1) {
        $target_id = reset(array_keys($entities['node']));
      }
    }
    return $target_id;
  }

  public function complete($entity, $row) {
    if (!empty($entity->og_group_ref)) {
      // Use pathauto
      $entity->path['pathauto'] = TRUE;
      node_save($entity);
    }
    if (isset($entity->body)) {
      $this->translateBody($entity);
    }
  }

  protected function redirect($entity, $row) {
    if ($this->arguments['source_connection'] != 'www' && isset($entity->path['alias'])) {
      $redirect = new stdClass();
      if (!isset($row->path)) {
        $row->path = 'node/'.$row->nid;
      }
      $redirect->source = $row->path;
      $redirect->redirect = 'https://www.humanitarianresponse.info/'.drupal_get_path_alias('node/'.$entity->nid);
      $redirect->language = LANGUAGE_NONE;
      $redirect->source_options = array();
      Database::getConnection('default', $this->sourceConnection)
        ->insert('redirect')
        ->fields(array(
          'hash' => redirect_hash($redirect),
          'type' => 'redirect',
          'uid' => 1,
          'source' => $redirect->source,
          'source_options' => serialize(array()),
          'redirect' => $redirect->redirect,
          'redirect_options' => serialize(array()),
          'language' => LANGUAGE_NONE,
          'status_code' => 0,
          'count' => 0,
          'access' => 0,
        ))
        ->execute();
    }
  }

  protected function translateFile($old_fid) {
    $migrations = migrate_migrations();
    $files_migration = $migrations['hr_files_'.$this->arguments['source_connection']];
    $map = $files_migration->map;
    $row_fid = $map->getRowBySource(array($old_fid));
    $new_fid = $row_fid['destid1'];
    return $new_fid;
  }

  protected function translateBody($entity) {
    $language = $entity->language;
    $body = $entity->body[$language][0]['value'];
    $body = str_replace('"view_mode":"media_responsive"', '"view_mode":"default"', $body);
    $body = str_replace('"format":"media_responsive"', '"format":"default"', $body);
    $body = preg_replace_callback('/"fid":"([0-9]*)"/', array($this, 'replaceBody'), $body);
    $entity->body[$language][0]['value'] = $body;
    node_save($entity);
  }

  protected function replaceBody($matches) {
    if (isset($matches[1])) {
      return '"fid":"'.$this->translateFile($matches[1]).'"';
    }
  }
}

/**
 * Migration class to test import of various date fields.
 */
class HRProfile2Migration extends HRMigration {

  public function __construct(array $arguments) {
    $arguments['destination_type'] = 'na'; // Destination type not used in this case
    parent::__construct($arguments);

    $fields = array(
      'title',
      'body',
      'status',
      'comment',
      'promote',
      'sticky',
      'body:language',
      'vid',
      'is_new',
      'revision',
      'log',
      'path',
      'pathauto',
      'body:summary',
      'body:format',
      'tnid',
      'translate',
      'language',
    );

    foreach ($fields as $field) {
      $this->removeFieldMapping($field, $field);
    }

    $this->sourceFields += $this->version->getSourceFields('profile2', 'main');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'pid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'alias' => 'p'
        )
      ),
      MigrateDestinationProfile2::getKeySchema()
    );

    $this->highwaterField = array(
      'name' => 'changed',
      'alias' => 'p',
      'type' => 'int',
    );


    $this->source = new MigrateSourceSQL($this->query(), $this->sourceFields, NULL, $this->sourceOptions);

    $this->destination = new MigrateDestinationProfile2('main');


    $this->addUnmigratedSources(array(
      'field_salutation',
      'path',
      'field_email',
      'pid',
      'type',
      'label',
      'field_profile_privacy',
      'field_themes',
      'field_country_you_work_in',
      'field_other_contact_info',
      'field_other_contact_info:revision_id',
    ));
    $this->addUnmigratedDestinations(array(
      'field_skype_id',
      'field_skype_id:language',
      'field_job_title_other',
      'field_job_title_other:language',
      'field_social_media',
      'field_social_media:title',
      'field_social_media:attributes',
      'field_social_media:language',
      'field_location',
    ));
    $this->addSimpleMappings(array('field_last_name', 'field_first_name', 'field_phones:countrycode', 'field_phones:extension'));
    $this->addFieldMapping('field_phones', 'field_phones:number');
    $this->addFieldMapping('field_phones:numbertype', 'field_phones');
    $this->addFieldMapping('field_emails', 'field_emails_other');
    $this->addFieldMapping('field_organizations_other', 'field_oganization_other');
    $this->addFieldMapping('field_organizations', 'field_organizations')->callbacks(array($this, 'fieldOrganizations'));
    $this->addFieldMapping('field_sectors', 'field_clusters')->callbacks(array($this, 'fieldSectors'));
    $this->addFieldMapping('field_job_title', 'field_job_title')->callbacks(array($this, 'fieldJobTitle')); 


  }

  public function query() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('profile', 'p')
             ->fields('p', array('pid', 'type', 'uid', 'label', 'created', 'changed'))
             ->isNotNull('p.uid')
             ->orderBy('changed');
    return $query;
  }


  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }

    $this->version->getSourceValues($row, $row->pid);
  }

  public function prepare($entity, $row) {
    // Make sure locations are not added
    $entity->field_location = array();
    if (empty($entity->uid)) {
      if (!empty($row->field_email[0])) {
        $user = user_load_by_mail($row->field_email[0]);
        if (empty($user)) {
          // Create user account
          $account = new stdClass();
          $account->is_new = TRUE;
          $account->mail = $row->field_email[0];
          $account->status = 1;
          $name = preg_replace('/@.*$/', '', $row->field_email[0]);
          $account->name = email_registration_unique_username($name);
          $user = user_save($account);
          if (!empty($user->uid)) {
            $entity->uid = $user->uid;
          }
          else {
            debug('Could not create user for '.$row->field_email[0]);
          }
        }
        else {
          $entity->uid = $user->uid;
        }
      }
      else {
        debug("Could not assign UID");
      }
    }
  }

  public function complete($entity, $row) {
    $user = user_load($entity->uid);
    // Resave user to update user realname
    user_save($user);
  }

}

class HRPagesMigration extends HRMigration {

  public function __construct($arguments) {
    parent::__construct($arguments);
    $this->removeFieldMapping('body:language');
    $this->addFieldMapping('body:language', 'language')->defaultValue('en');
    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('title_field:language', 'language')->defaultValue('en');
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] == 'www') {
      $valid_paths = array(
        'coordination/humanitarian-leadership' => 'Humanitarian Leadership',
        'themes/gencap' => 'GenCap',
        'themes/procap' => 'ProCap',
        'themes/accountability-to-affected-people' => 'Accountability to Affected People',
        'themes/age' => 'Age',
        'themes/environment' => 'Environment',
        'themes/gender' => 'Gender',
      );
      foreach ($valid_paths as $path => $space_title) {
        if (strpos($row->path, $path) !== FALSE) {
          $space = NULL;
          // Get space id
          $query = new EntityFieldQuery();
          $entities = $query
            ->entityCondition('entity_type', 'node')
            ->entityCondition('bundle', 'hr_space')
            ->propertyCondition('title', $space_title)
            ->execute();
          if (!empty($entities['node'])) {
            if (count($entities['node']) == 1) {
              $space = reset(array_keys($entities['node']));
            }
          }
          if (!empty($space)) {
            $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $space;
          }
        }
      }
    }
    else {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    if (!empty($entity->og_group_ref)) {
      // Use pathauto
      $entity->path['pathauto'] = TRUE;
      node_save($entity);
      // Add group menu items
      $items['Humanitarian Leadership'] = array(
        'HLSU Who and What We Do' => array('link_title' => 'Who we are and What we do', 'weight' => 0),
        'HC Pool' => array('link_title' => 'HC Pool', 'weight' => 2),
        'Level 3 HC Pool' => array('link_title' => 'Level 3 HC Pool', 'weight' => 4),
        'Humanitarian Country Team Development' => array('link_title' => 'Humanitarian Country Team Development', 'weight' => 6),
        'Learning Development' => array('link_title' => 'Learning Development', 'weight' => 8),
      );

      $items['GenCap'] = array(
        'GenCap - What we do' => array('link_title' => 'What we do', 'weight' => 0),
        'GenCap - Who we are' => array('link_title' => 'Who we are', 'weight' => 2),
        'GenCap - Where we are' => array('link_title' => 'Where we are', 'weight' => 4),
        'How to request a GenCap?' => array('link_title' => 'How to request a GenCap', 'weight' => 6),
        'How to become a GenCap?' => array('link_title' => 'How to become a GenCap', 'weight' => 8),
        'GenCap - Project Reports' => array('link_title' => 'Project Reports', 'weight' => 10),
        'GenCap - Guidance' => array('link_title' => 'Guidance', 'weight' => 12),
        'GenCap - Videos' => array('link_title' => 'Videos', 'weight' => 14),
        'GenCap - Updates' => array('link_title' => 'Updates', 'weight' => 16),
        'Online Training : Different Needs - Equal Opportunities - In English and French' => array('link_title' => 'Online course', 'weight' => 18),
      );

      $items['ProCap'] = array(
        'ProCap - What we do' => array('link_title' => 'What we do', 'weight' => 0),
        'ProCap - Who we are' => array('link_title' => 'Who we are', 'weight' => 2),
        'ProCap - Where we are' => array('link_title' => 'Where we are', 'weight' => 4),
        'ProCap - How to request a ProCap' => array('link_title' => 'How to request a ProCap', 'weight' => 6),
        'ProCap - How to become a ProCap' => array('link_title' => 'How to become a ProCap', 'weight' => 8),
        'Inter-Agency Protection Capacity Training' => array('link_title' => 'ProCap Training', 'weight' => 10),
        'ProCap - Project Reports' => array('link_title' => 'Project Reports', 'weight' => 12),
        'ProCap - Guidance' => array('link_title' => 'Guidance', 'weight' => 14),
        'ProCap - Videos' => array('link_title' => 'Videos', 'weight' => 16),
        'ProCap - Updates' => array('link_title' => 'Updates', 'weight' => 18),
      );

      $items['Gender'] = array(
        'Training' => array('link_title' => 'Training', 'weight' => 0),
        'The IASC Gender Marker' => array('link_title' => 'The IASC Gender Marker', 'weight' => 2),
      );


      $space_id = $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'];
      $space = node_load($space_id);
      if (isset($items[$space->title]) && isset($items[$space->title][$entity->title])) {
        $menu_item = $items[$space->title][$entity->title];
        $menus = og_menu_get_group_menus(array('node' => array($space->nid)));
        if (!empty($menus)) {
          $menu_item['menu_name'] = $menus[0]['menu_name'];
          $menu_item['link_path'] = 'node/'.$entity->nid;
          menu_link_save($menu_item);
        }
      }
    }
    $this->redirect($entity, $row);
  }
} 

class HRDocumentMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    
    $this->addUnmigratedSources(array(
      'field_document_thumbnail',
      'field_document_thumbnail:alt',
      'field_document_thumbnail:title',
      'field_document_thumbnail:width',
      'field_document_thumbnail:height',
      'field_page',
      'field_display_properties',
      'field_document_files',
      'field_document_files:revision_id',
    ));
    $this->addUnmigratedDestinations(array(
      'field_publication_date:rrule',
      'field_publication_date:to',
      'field_files_collection',
    ));

    $this->removeFieldMapping('body:language');
    $this->addFieldMapping('body:language', 'language')->defaultValue('en');

    $this->addFieldMapping('group_content_access')->defaultValue(0);

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_publication_date', 'field_publication_date');
    $this->addFieldMapping('field_publication_date:timezone')
      ->defaultValue(date_default_timezone_get());
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_document_type', 'field_document_type')->callbacks(array($this, 'fieldDocumentTypes'));
    $this->addFieldMapping('field_organizations', 'field_organizations')->callbacks(array($this, 'fieldOrganizations'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_bundles', 'field_clusters')->callbacks(array($this, 'fieldBundles'));
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_funding_methods', 'field_fundings')->callbacks(array($this, 'fieldFundingMethods'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    if ($this->arguments['source_connection'] == 'www') {
      $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
    }
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
  }
}

/**
 * Migration for infographics
 */
class HRInfographicMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $files_migration = 'hr_files';

    if ($this->arguments['source_connection'] != 'www') {
      $files_migration = 'hr_files_'.$this->arguments['source_connection'];
    }

    $this->dependencies[] = $files_migration;

    $this->addUnmigratedSources(array(
      'field_doc_thumbnail',
      'field_doc_thumbnail:alt',
      'field_doc_thumbnail:title',
      'field_doc_thumbnail:width',
      'field_doc_thumbnail:height',
      'field_display_properties',
    ));
    $this->addUnmigratedDestinations(array(
      'field_publication_date:rrule',
      'field_publication_date:to',
      'field_files_collection',
    ));

    $this->addFieldMapping('group_content_access')->defaultValue(0);

    $this->removeFieldMapping('body:language');
    $this->addFieldMapping('body:language', 'language')->defaultValue('en');

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_publication_date', 'field_publication_date');
    $this->addFieldMapping('field_publication_date:timezone')
      ->defaultValue(date_default_timezone_get());
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_infographic_type', 'field_map_type')->callbacks(array($this, 'fieldMapTypes'));
    $this->addFieldMapping('field_organizations', 'field_organizations')->callbacks(array($this, 'fieldOrganizations'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_bundles', 'field_clusters')->callbacks(array($this, 'fieldBundles'));
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_funding_methods', 'field_fundings')->callbacks(array($this, 'fieldFundingMethods'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $new_fid = $this->translateFile($row->field_map_file[0]);
    $node = node_load($entity->nid);
    $field_collection_item = entity_create('field_collection_item', array('field_name' => 'field_files_collection'));
    $field_collection_item->setHostEntity('node',$node);
    $field_collection_item->field_language[LANGUAGE_NONE][0]['value'] = $row->field_file_languages[0];
    $field_collection_item->field_file[LANGUAGE_NONE][0]['fid'] = $new_fid;
    $field_collection_item->field_file[LANGUAGE_NONE][0]['display'] = 1;
    $field_collection_item->save();
    $this->redirect($entity, $row);
  }
}


/*
 * Migration class for news 
 */
class HRNewsMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    if ($this->arguments['source_connection'] == 'www') {
      $this->dependencies[] = 'hr_files';
    }
    else {
      $this->dependencies[] = 'hr_files_'.$this->arguments['source_connection'];
    }

    $this->addUnmigratedSources(array(
      'field_page',
      'field_news_image:width',
      'field_news_image:height',
    ));
    $this->addUnmigratedDestinations(array(
      'field_organizations',
    ));

    $this->removeFieldMapping('body:language');
    $this->addFieldMapping('body:language', 'language')->defaultValue('en');

    $this->addFieldMapping('group_content_access')->defaultValue(0);

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_bundles', 'field_clusters')->callbacks(array($this, 'fieldBundles'));
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    if ($this->arguments['source_connection'] == 'www') {
      $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
      $this->addFieldMapping('field_image', 'field_news_image')->sourceMigration('hr_files');
    }
    else {
      $this->addFieldMapping('field_image', 'field_news_image')->sourceMigration('hr_files_'.$this->arguments['source_connection']);
    }
    $this->addFieldMapping('field_funding_methods', 'field_fundings')->callbacks(array($this, 'fieldFundingMethods'));

    $this->addFieldMapping('field_image:alt', 'field_news_image:alt');
    $this->addFieldMapping('field_image:title', 'field_news_image:title');
    $this->addFieldMapping('field_image:file_class')->defaultValue('MigrateFileFid');
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
  }


}

/*
 * Migration class for news 
 * @todo Meeting minutes and agenda
 */
class HREventsMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->addUnmigratedSources(array(
      'field_page',
      'field_event_map',
      'field_event_map:lat',
      'field_event_map:lon',
      'field_event_map:left',
      'field_event_map:top',
      'field_event_map:right',
      'field_event_map:bottom',
      'field_event_map:geom',
      'field_event_map:geohash'
    ));
    $this->addUnmigratedDestinations(array(
      'field_organizations',
    ));

    $this->removeFieldMapping('body:language');
    $this->addFieldMapping('body:language', 'language')->defaultValue('en');
    $this->addFieldMapping('group_content_access')->defaultValue(0);

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_bundles', 'field_clusters')->callbacks(array($this, 'fieldBundles'));
    $this->addFieldMapping('field_location', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    $this->addFieldMapping('field_funding_methods', 'field_fundings')->callbacks(array($this, 'fieldFundingMethods'));

    $this->addFieldMapping('field_address', 'field_event_address');
    $this->addFieldMapping('field_address:administrative_area', 'field_event_address:administrative_area');
    $this->addFieldMapping('field_address:sub_administrative_area', 'field_event_address:sub_administrative_area');
    $this->addFieldMapping('field_address:locality', 'field_event_address:locality');
    $this->addFieldMapping('field_address:dependent_locality', 'field_event_address:dependent_locality');
    $this->addFieldMapping('field_address:postal_code', 'field_event_address:postal_code');
    $this->addFieldMapping('field_address:thoroughfare', 'field_event_address:thoroughfare');
    $this->addFieldMapping('field_address:premise', 'field_event_address:premise');
    $this->addFieldMapping('field_address:sub_premise', 'field_event_address:sub_premise');
    $this->addFieldMapping('field_address:organisation_name', 'field_event_address:organisation_name');
    $this->addFieldMapping('field_address:name_line', 'field_event_address:name_line');
    $this->addFieldMapping('field_address:first_name', 'field_event_address:first_name');
    $this->addFieldMapping('field_address:last_name', 'field_event_address:last_name');
    $this->addFieldMapping('field_address:data', 'field_event_address:data');

    $this->addFieldMapping('field_event_date', 'field_event_date');
    $this->addFieldMapping('field_event_date:rrule', 'field_event_date:rrule');
    $this->addFieldMapping('field_event_date:to', 'field_event_date:value2');
    $this->addFieldMapping('field_event_date:timezone')->defaultValue(date_default_timezone_get());

    $this->addFieldMapping('field_event_category', 'field_event_category')->callbacks(array($this, 'fieldEventCategory'));

    if ($this->arguments['source_connection'] == 'www') {
      $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
    }
    
  }

  protected function fieldEventCategory($value) {
    return $this->translateTerms($value, 'hr_event_category');
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
    if (isset($row->field_event_contact_person) || isset($row->field_event_contact_email) || isset($row->field_event_contact_telephone)) {
      $node = node_load($entity->nid);
      $field_collection_item = entity_create('field_collection_item', array('field_name' => 'field_users_ref'));
      $field_collection_item->setHostEntity('node',$node);
      $field_collection_item->field_users_ref_checkbox[LANGUAGE_NONE][0]['value'] = 1;
      if (isset($row->field_event_contact_person)) {
        $field_collection_item->field_users_ref_name[LANGUAGE_NONE][0]['value'] = $row->field_event_contact_person[0];
      }
      if (isset($row->field_event_contact_email)) {
        $field_collection_item->field_email[LANGUAGE_NONE][0]['email'] = $row->field_event_contact_email[0];
      }
      if (isset($row->field_event_contact_telephone)) {
        $phones = explode('/', $row->field_event_contact_telephone[0]);
        $field_collection_item->field_phones[LANGUAGE_NONE][0]['number'] = '+'.str_replace(array(' ','-'), '', $phones[0]);
      }
      $field_collection_item->save();
    }
    if (isset($row->field_event_minutes) && $row->field_event_minutes != NULL) {
      debug('Event with meeting minutes');
    }
    if (isset($row->field_event_agenda) && $row->field_event_agenda != NULL) {
      debug('Event with agenda');
    }
  }

}


/**
 * Migration class for document field collection files
 */
class HRFilesCollectionMigration extends DrupalMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $documents_migration = 'hr_documents';
    $files_migration = 'hr_files';

    if ($this->arguments['source_connection'] != 'www') {
      $documents_migration = 'hr_documents_'.$this->arguments['source_connection'];
      $files_migration = 'hr_files_'.$this->arguments['source_connection'];
    }

    $this->dependencies[] = $documents_migration;
    $this->dependencies[] = $files_migration;

    $this->sourceFields += $this->version->getSourceFields('field_collection_item', 'field_document_files');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'field_document_files_value' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'alias' => 'f'
        )
      ),
      MigrateDestinationFieldCollection::getKeySchema()
    );

    $this->source = new MigrateSourceSQL($this->query(), $this->sourceFields, NULL, $this->sourceOptions);

    $this->destination = new MigrateDestinationFieldCollection(
      'field_files_collection',
      array('host_entity_type' => 'node')
    );

    $this->addUnmigratedSources(array('language'));

    $this->addFieldMapping('host_entity_id', 'entity_id')
      ->sourceMigration($documents_migration);

    $this->addFieldMapping('field_language', 'field_file_languages');

    $this->addFieldMapping('field_file', 'field_document_files_file')->sourceMigration($files_migration);
    $this->addFieldMapping('field_file:display', 'field_document_files_file:display');
    $this->addFieldMapping('field_file:description', 'field_document_files_file:description');
    $this->addFieldMapping('field_file:file_class')->defaultValue('MigrateFileFid');

  }

  public function query() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('field_data_field_document_files', 'f')
             ->fields('f', array('entity_id', 'language', 'field_document_files_value'));
    return $query;
  }

  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }
    $this->version->getSourceValues($row, $row->field_document_files_value);
  }

  protected function fieldLanguage($value) {
    return $value[0];
  }

}

class HRFileMigration extends DrupalFile7Migration {
  protected $site = '';

  public function __construct(array $arguments) {
    parent::__construct($arguments);
    if (isset($arguments['site'])) {
      $this->site = $arguments['site'];
    }
    $this->removeFieldMapping('value');
    $this->removeFieldMapping('destination_file');
    $this->addFieldMapping('destination_file', 'filename');
    $this->addFieldMapping('value', 'uri')->callbacks(array($this, 'fileValue'));

    $this->removeFieldMapping('destination_dir');
    $this->addFieldMapping('destination_dir', 'uri')->callbacks(array($this, 'destinationDir'));

    $this->removeFieldMapping('source_dir');
    $this->addFieldMapping('source_dir', 'uri')->callbacks(array($this, 'sourceDir'));

    $this->removeFieldMapping('preserve_files');
    $this->addFieldMapping('preserve_files')->defaultValue(FALSE);
  }

  protected function destinationDir($uri) {
    $scheme = '';
    $result = '';
    // Public or private ?
    if (strpos($uri, 'public://') !== FALSE) {
      $result = str_replace('public://', '', $uri);
      $scheme = 'public://';
    }
    else {
      $result = str_replace('private://', '', $uri);
      $scheme = 'private://';
    }
    $results = explode('/', $result);
    array_pop($results);
    $result = implode('/', $results);
    return $scheme.$result;
  }

  protected function sourceDir($uri) {
    $base = '';
    $result = '';
    // Public or private ?
    if (strpos($uri, 'public://') !== FALSE) {
      $base = 'https://'.$this->site.'/sites/'.$this->site.'/files/';
      $result = str_replace('public://', '', $uri);
    }
    else {
      $base = 'https://'.$this->site.'/system/files/';
      $result = str_replace('private://', '', $uri);
    }
    $results = explode('/', $result);
    array_pop($results);
    $result = implode('/', $results);
    return $base.$result;
  }

  protected function fileValue($uri) {
    if (strpos($uri, 'public://') !== FALSE) {
      $result = str_replace('public://', '', $uri);
    }
    else {
      $result = str_replace('private://', '', $uri);
    }
    $results = explode('/', $result);
    return array_pop($results);
  }
}

class HRIndicatorsMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->addSimpleMappings(array('title_field', 'field_ind_code', 'field_ind_comments', 'field_ind_data_sources', 'field_ind_denominator', 'field_ind_disaggregation', 'field_ind_general_guidance',
      'field_ind_guidance_phases', 'field_ind_key', 'field_ind_numerator', 'field_ind_response', 'field_ind_unit_desc', 'field_ind_guidance_baseline', 'field_ind_threshold'));

    $this->addFieldMapping('field_sector', 'field_cluster')->callbacks(array($this, 'fieldSectors'));
    $this->addFieldMapping('field_ind_domain', 'field_ind_domain')->callbacks(array($this, 'fieldIndDomain'));
    $this->addFieldMapping('field_ind_standards', 'field_ind_standards')->callbacks(array($this, 'fieldIndStandards'));
    $this->addFieldMapping('field_ind_types', 'field_ind_types')->callbacks(array($this, 'fieldIndTypes'));
    $this->addFieldMapping('field_ind_unit', 'field_ind_unit')->callbacks(array($this, 'fieldIndUnit'));
    $this->addFieldMapping('field_ind_cross_tagging', 'field_cross_tagging')->callbacks(array($this, 'fieldIndDomain'));

  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
  }

  protected function fieldIndDomain($value) {
    return $this->translateTerms($value, 'hr_indicator_domain');
  }

  protected function fieldIndStandards($value) {
    return $this->translateTerms($value, 'hr_indicator_standard');
  }

  protected function fieldIndTypes($value) {
    return $this->translateTerms($value, 'hr_indicator_type');
  }

  protected function fieldIndUnit($value) {
    return $this->translateTerms($value, 'hr_indicator_unit');
  }

}


class HRTrainingsMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->removeFieldMapping('body:language');
    $this->addFieldMapping('body:language', 'language')->defaultValue('en');

    $this->addFieldMapping('title_field', 'title_field');
    $dummy = array();
    $themes = $this->fieldThemes($dummy);
    $this->addFieldMapping('field_themes')->defaultValue($themes);

    $this->addFieldMapping('field_event_date', 'field_training_date');
    $this->addFieldMapping('field_event_date:to', 'field_training_date:value2');
    $this->addFieldMapping('field_event_date:timezone')->defaultValue(date_default_timezone_get());
    $dummy = 2;
    $this->addFieldMapping('field_event_category')->defaultValue($this->fieldEventCategory($dummy));


  }

  protected function fieldEventCategory($value) {
    $terms = taxonomy_get_term_by_name('Trainings', 'hr_event_category');
   if (!empty($terms)) {
     $keys = array_keys($terms);
     $return = reset($keys);
     return $return;
   }
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
  }

}

class HRDatasetsMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $files_migration = 'hr_files_cod';
    $this->dependencies[] = 'hr_files_cod';

    $this->removeFieldMapping('body');
    $this->removeFieldMapping('body:summary');
    $this->removeFieldMapping('body:format');
    $this->removeFieldMapping('body:language');

    $this->addFieldMapping('field_ds_instructions', 'field_instructions');
    $this->addFieldMapping('field_ds_instructions:format', 'field_instructions:format')->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('field_ds_abstract', 'field_abstract');
    $this->addFieldMapping('field_ds_abstract:format', 'field_abstract:format')->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('field_ds_recent_changes', 'field_recent_changes');
    $this->addFieldMapping('field_ds_recent_changes:format', 'field_recent_changes:format')->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('field_locations', 'field_country_region')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_dataset_categories', 'field_category')->callbacks(array($this, 'fieldDatasetCategory'));
    $this->addFieldMapping('field_dataset_types', 'field_data_type')->callbacks(array($this, 'fieldDatasetType'));
    $this->addFieldMapping('field_languages', 'field_language')->callbacks(array($this, 'fieldDatasetLanguage'));
    $this->addFieldMapping('field_ds_date', 'field_dataset_date')->callbacks(array($this, 'fieldDatasetDate'));
    $this->addFieldMapping('field_ds_date:timezone')->defaultValue(date_default_timezone_get());
    //$this->addFieldMapping('field_ds_date')->defaultValue('2001-01-01');
    $this->addFieldMapping('field_ds_datasources', 'field_data_source_s_');
    $this->addFieldMapping('field_ds_datasources:format', 'field_data_source_s_:format')->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('field_ds_summary', 'field_summary');
    $this->addFieldMapping('field_ds_summary:format', 'field_summary:format')->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('field_terms_of_use', 'field_terms_of_use');
    $this->addFieldMapping('field_terms_of_use:format', 'field_terms_of_use:format')->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('field_email', 'field_contact_point')->callbacks(array($this, 'fieldDatasetEmail'));

    $this->addFieldMapping('field_ds_files', 'field_dataset_file')->sourceMigration($files_migration);
    $this->addFieldMapping('field_ds_files:display', 'field_dataset_file:display');
    $this->addFieldMapping('field_ds_files:description', 'field_dataset_file:description');
    $this->addFieldMapping('field_ds_files:file_class')->defaultValue('MigrateFileFid');

  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity_id = 0;
      $tid = $row->field_country_region[0];
      $term_name = $this->getLegacyTermName($tid);
      switch ($term_name) {
        case "Korea, Democratic People's Republic of":
          $term_name = 'North Korea';
          break;
        case 'Palestinian Territory, Occupied':
          $term_name = 'State of Palestine';
          break;
        case "Lao People's Democratic Republic":
          $term_name = 'Lao';
          break;
        case 'Congo, The Democratic Republic of the':
          $term_name = 'Democratic Republic of the Congo';
          break;
        case 'Dominican Republic (the)':
          $term_name = 'Dominican Republic';
          break;
        case 'Syrian Arab Republic':
          $term_name = 'Syria';
          break;
        case 'Guinea-Bissau':
          $term_name = 'Guinea Bissau';
          break;
        case 'Tanzania, United Republic of':
          $term_name = 'Tanzania';
          break;
      }
      $query = new EntityFieldQuery();
      $entities = $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'hr_operation')
        ->propertyCondition('title', $term_name)
        ->execute();
      if (!empty($entities['node'])) {
        if (count($entities['node']) == 1) {
          $entity_id = reset(array_keys($entities['node']));
        }
      }
      if ($entity_id) {
       $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $entity_id;
      }
      else {
        debug('Could not find operation with title '.$term_name);
        $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
      }
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
  }

  protected function fieldDatasetCategory($value) {
    return $this->translateTerms($value, 'hr_dataset_category');
  }

  protected function fieldDatasetType($value) {
    return $this->translateTerms($value, 'hr_dataset_type');
  }

  protected function fieldDatasetLanguage($value) {
    $out = array();
    foreach ($value as $val) {
      $term = $this->getLegacyTermName($val);
      switch ($term) {
        case 'English':
          $out[] = 'en';
          break;
        case 'French':
          $out[] = 'fr';
          break;
        case 'Spanish':
          $out[] = 'es';
          break;
        case 'Arabic':
          $out[] = 'ar';
          break;
        case 'Russian':
          $out[] = 'ru';
          break;
        case 'Other':
          debug('Other');
          break;
      }
    }
    return $out;
  }

  protected function fieldDatasetDate($value) {
    $pattern = '/^\d{4}$/';
    $out = array();
    foreach ($value as $val) {
      if (preg_match($pattern, $val)) {
        $val .= '-01-01';
      }
      $time = strtotime($val);
      if ($time === FALSE) {
        debug('Could not parse '.$val);
      }
      else {
        $out[] = date('Y-m-d', $time);
      }
    }
    return $out;
  }

  protected function fieldDatasetEmail($value) {
    $pattern = '/[a-z\d._%+-]+@[a-z\d.-]+\.[a-z]{2,4}\b/i';
    $out = array();
    foreach ($value as $val) {
      $matches = array();
      if (preg_match_all($pattern, $val, $matches) > 0) {
        $out[] = $matches[0];
      }
    }
    if (empty($out)) {
      debug('Could not find email in '.$val);
      $out[] = 'data@humanitarianresponse.info';
    }
    return $out;
  }

}

class HRContactsMigration extends HRMigration {

  public function __construct(array $arguments) {
    $arguments['destination_type'] = 'na'; // Destination type not used in this case
    parent::__construct($arguments);

    $fields = array(
      'title',
      'body',
      'status',
      'comment',
      'promote',
      'sticky',
      'body:language',
      'vid',
      'is_new',
      'revision',
      'log',
      'path',
      'pathauto',
      'body:summary',
      'body:format',
      'tnid',
      'translate',
      'language',
    );

    foreach ($fields as $field) {
      $this->removeFieldMapping($field, $field);
    }

    $this->sourceFields += $this->version->getSourceFields('profile2', 'main');

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'pid' => array(
          'type' => 'int',
          'unsigned' => TRUE,
          'not null' => TRUE,
          'alias' => 'p'
        )
      ),
      MigrateDestinationProfile2::getKeySchema()
    );

    $this->highwaterField = array(
      'name' => 'changed',
      'alias' => 'p',
      'type' => 'int',
    );


    $this->source = new MigrateSourceSQL($this->query(), $this->sourceFields, NULL, $this->sourceOptions);

    $this->destination = new MigrateDestinationNode('hr_contact');
    $this->addUnmigratedSources(array(
      'field_salutation',
      'path',
      'pid',
      'type',
      'label',
      'field_profile_privacy',
      'field_other_contact_info',
    ));
    $this->addUnmigratedDestinations(array(
      'field_skype_id',
      'field_skype_id:language',
      'field_job_title_other',
      'field_job_title_other:language',
      'field_social_media',
      'field_social_media:title',
      'field_social_media:attributes',
      'field_social_media:language',
      'field_locations',
    ));
    $this->addSimpleMappings(array('field_email', 'field_last_name', 'field_first_name', 'field_phones:countrycode', 'field_phones:extension', 'field_opt_out'));
    $this->addFieldMapping('field_location', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_phones', 'field_phones:number');
    $this->addFieldMapping('field_phones:numbertype', 'field_phones');
    $this->addFieldMapping('field_emails', 'field_emails_other');
    $this->addFieldMapping('field_organizations_other', 'field_oganization_other');
    $this->addFieldMapping('field_organizations', 'field_organizations')->callbacks(array($this, 'fieldOrganizations'));
    $this->addFieldMapping('field_bundles', 'field_clusters')->callbacks(array($this, 'fieldBundles'));
    $this->addFieldMapping('field_job_title', 'field_job_title')->callbacks(array($this, 'fieldJobTitle'));
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_funding_methods', 'field_fundings')->callbacks(array($this, 'fieldFundingMethods'));

  }

  public function query() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('profile', 'p')
             ->fields('p', array('pid', 'type', 'uid', 'label', 'created', 'changed'))
             ->orderBy('changed');
    return $query;
  }

  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }

    $this->version->getSourceValues($row, $row->pid);
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $row->path = 'profile/'.$row->pid;
    $this->redirect($entity, $row);
    if (isset($row->uid) && !is_null($row->uid)) {
      $gid = $this->getSpace();
      $user = user_load($entity->uid);
      // Ungroup user
      og_ungroup('node', $gid, 'user', $entity->uid);
      //Make user a responder of group
      og_group('node', $gid, array('entity_type' => 'user', 'entity' => $user, 'field_name' => 'field_responder_in', 'field_contact_information' => $entity->nid));
    }
  }

}

/**
 * Migration of assessments
 */
class HRAssessmentMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    /*$this->addUnmigratedSources(array(
      'field_doc_thumbnail',
      'field_doc_thumbnail:alt',
      'field_doc_thumbnail:title',
      'field_doc_thumbnail:width',
      'field_doc_thumbnail:height',
      'field_display_properties',
    ));
    $this->addUnmigratedDestinations(array(
      'field_publication_date:rrule',
      'field_publication_date:to',
      'field_files_collection',
    ));*/

    $this->removeFieldMapping('body');
    $this->removeFieldMapping('body:summary');
    $this->removeFieldMapping('body:format');
    $this->removeFieldMapping('body:language');

    $this->addSimpleMappings(array('field_asst_other_location', 'field_asst_subject', 'field_asst_methodology', 'field_asst_key_findings', 'field_asst_unit', 'field_asst_collection_method',
      'field_asst_sample_size', 'field_asst_freq', 'field_asst_status'));

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_organizations', 'field_organizations')->callbacks(array($this, 'fieldOrganizations'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    $this->addFieldMapping('field_organizations2', 'field_asst_participating_org')->callbacks(array($this, 'fieldOrganizations'));
    $this->addFieldMapping('field_asst_date', 'field_asst_start_date');
    $this->addFieldMapping('field_asst_date:timezone')->defaultValue(date_default_timezone_get());
  }

  public function prepare($entity, $row) {
    if ($this->arguments['source_connection'] != 'www') {
      $entity->og_group_ref[LANGUAGE_NONE][0]['target_id'] = $this->getSpace();
    }
  }

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $this->redirect($entity, $row);
  }
}

class HRUserMigration extends DrupalUser7Migration {

  public function complete($entity, $row) {
    parent::complete($entity, $row);
    $gid = $this->getSpace();
    //Make user a member of group
    og_group('node', $gid, array('entity_type' => 'user', 'entity' => $entity, 'field_name' => 'og_user_node'));
  }

  protected function getSpace() {
    $operations = array(
      'ethiopia' => 'Ethiopia',
    );
    if (in_array($this->arguments['source_connection'], array_keys($operations))) {
      $type = 'hr_operation';
      $title = $operations[$this->arguments['source_connection']];
    }
    $query = new EntityFieldQuery();
    $entities = $query
      ->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $type)
      ->propertyCondition('title', $title)
      ->execute();
    if (!empty($entities['node'])) {
      if (count($entities['node']) == 1) {
        $target_id = reset(array_keys($entities['node']));
      }
    }
    return $target_id;
  }
}
