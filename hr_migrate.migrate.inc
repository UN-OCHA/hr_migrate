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
    return $return;
  }

  protected function translateNodes($value, $type) {
    if (!is_array($value)) {
      $value = array($value);
    }
    $return  = array();
    foreach ($value as $i => $val) {
      $term_name = $this->getLegacyTermName($val);
      if (!empty($term_name)) {
        $query = new EntityFieldQuery();
        $entities = $query
          ->entityCondition('entity_type', 'node')
          ->entityCondition('bundle', $type)
          ->propertyCondition('title', $term_name)
          ->execute();
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

  protected function fieldOrganizations($value) {
    return $this->translateTerms($value, 'hr_organization');
  }

  protected function fieldSectors($value) {
    return $this->translateNodes($value, 'hr_sector');
  }

  protected function fieldLocations($value) {
    return $this->translateTerms($value, 'hr_location');
  }

  protected function fieldFundings($value) {
    return $this->translateTerms($value, 'hr_funding_type');
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
    );
    $query = new EntityFieldQuery();
    $entities = $query
      ->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', 'hr_space')
      ->propertyCondition('title', $groups[$this->arguments['source_connection']])
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
  }

  protected function redirect($entity, $row) {
    if ($this->arguments['source_connection'] != 'www' && isset($entity->path['alias'])) {
      $redirect = new stdClass();
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
    $this->addFieldMapping('field_sectors', 'field_clusters')->callbacks(array($this, 'fieldSectors'));
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_funding_types', 'field_fundings')->callbacks(array($this, 'fieldFundings'));
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

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_sectors', 'field_clusters')->callbacks(array($this, 'fieldSectors'));
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    if ($this->arguments['source_connection'] == 'www') {
      $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
      $this->addFieldMapping('field_image', 'field_news_image')->sourceMigration('hr_files');
    }
    else {
      $this->addFieldMapping('field_image', 'field_news_image')->sourceMigration('hr_files_'.$this->arguments['source_connection']);
    }
    $this->addFieldMapping('field_funding_types', 'field_fundings')->callbacks(array($this, 'fieldFundings'));

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

    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_sectors', 'field_clusters')->callbacks(array($this, 'fieldSectors'));
    $this->addFieldMapping('field_location', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    $this->addFieldMapping('field_funding_types', 'field_fundings')->callbacks(array($this, 'fieldFundings'));

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

    $this->addFieldMapping('field_event_email', 'field_event_contact_email');
    $this->addFieldMapping('field_event_person', 'field_event_contact_person');
    $this->addFieldMapping('field_event_person:language')->defaultValue(LANGUAGE_NONE);
    $this->addFieldMapping('field_event_telephone', 'field_event_contact_telephone'); // @todo Add callback to transform text field in valid phone

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
    $this->addFieldMapping('value', 'filename');

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

