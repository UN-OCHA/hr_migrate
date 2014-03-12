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
    $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
  }

}

/*
 * Migration class for news 
 */
class HRNewsMigration extends HRMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->dependencies[] = 'hr_files';

    $this->addUnmigratedSources(array(
      'field_page',
      'field_news_image:width',
      'field_news_image:height',
    ));
    $this->addUnmigratedDestinations(array(
      'field_organizations',
    ));
    $this->addFieldMapping('title_field', 'title_field');
    $this->addFieldMapping('field_themes', 'field_themes')->callbacks(array($this, 'fieldThemes'));
    $this->addFieldMapping('field_related_content', 'field_linked_resources');
    $this->addFieldMapping('field_related_content:title', 'field_linked_resources:title');
    $this->addFieldMapping('field_related_content:attributes', 'field_linked_resources:attributes');
    $this->addFieldMapping('field_sectors', 'field_clusters')->callbacks(array($this, 'fieldSectors'));
    $this->addFieldMapping('field_locations', 'field_locations')->callbacks(array($this, 'fieldLocations'));
    $this->addFieldMapping('field_related_content:language')->defaultValue(LANGUAGE_NONE);
    $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
    $this->addFieldMapping('field_funding_types', 'field_fundings')->callbacks(array($this, 'fieldFundings'));

    $this->addFieldMapping('field_image', 'field_news_image')->sourceMigration('hr_files');
    $this->addFieldMapping('field_image:alt', 'field_news_image:alt');
    $this->addFieldMapping('field_image:title', 'field_news_image:title');
    $this->addFieldMapping('field_image:file_class')->defaultValue('MigrateFileFid');
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

    $this->addFieldMapping('og_group_ref', 'field_themes')->callbacks(array($this, 'fieldGroupAudience'));
    
  }

  protected function fieldEventCategory($value) {
    return $this->translateTerms($value, 'hr_event_category');
  }

}


/**
 * Migration class for document field collection files
 */
class HRFilesCollectionMigration extends DrupalMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    $this->dependencies[] = 'hr_documents';
    $this->dependencies[] = 'hr_files';

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
      ->sourceMigration('hr_documents');

    $this->addFieldMapping('field_language', 'field_file_languages');

    $this->addFieldMapping('field_file', 'field_document_files_file')->sourceMigration('hr_files');
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
