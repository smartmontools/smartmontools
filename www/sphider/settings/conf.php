<?php 
/***********************
 Sphider configuration file
***********************/


/*********************** 
General settings 
***********************/

// Sphider version 
$version_nr			= '1.3.4';

//Language of the search page 
$language			= 'en';

// Template name/directory in templates dir
$template	= 'standard';

//Administrators email address (logs can be sent there)	
$admin_email		= '<email>';

// Print spidering results to standard out
$print_results		= 1;

// Temporary directory, this should be readable and writable
$tmp_dir	= 'tmp';


/*********************** 
Logging settings 
***********************/

// Should log files be kept
$keep_log			= 0;

//Log directory, this should be readable and writable
$log_dir	= 'log';

// Log format
$log_format			= 'html';

//  Send log file to email 
$email_log			= 0;


/*********************** 
Spider settings 
***********************/

// Min words per page required for indexing 
$min_words_per_page = 10;

// Words shorter than this will not be indexed
$min_word_length	= 3;

// Keyword weight depending on the number of times it appears in a page is capped at this value
$word_upper_bound	= 100;

// Index numbers as well
$index_numbers		= 1;

// if this value is set to 1, word in domain name and url path are also indexed,// so that for example the index of www.php.net returns a positive answer to query 'php' even 	// if the word is not included in the page itself.
$index_host		 = 0;


// Wether to index keywords in a meta tag 
$index_meta_keywords = 1;

// Index pdf files
$index_pdf	= 0;

// Index doc files
$index_doc	= 0;

// Index xls files
$index_xls	= 0;

// Index ppt files
$index_ppt	= 0;

//executable path to pdf converter
$pdftotext_path	= 'c:\temp\pdftotext.exe';

//executable path to doc converter
$catdoc_path	= 'c:\temp\catdoc.exe';

//executable path to xls converter
$xls2csv_path	= 'c:\temp\xls2csv';

//executable path to ppt converter
$catppt_path	= 'c:\temp\catppt';

// User agent string 
$user_agent			 = 'Sphider';

// Minimal delay between page downloads 
$min_delay			= 0;

// Use word stemming (e.g. find sites containing runs and running when searching for run) 
$stem_words			= 0;

// Strip session ids (PHPSESSID, JSESSIONID, ASPSESSIONID, sid) 
$strip_sessids			= 1;


/*********************** 
Search settings 
***********************/

// default for number of results per page
$results_per_page	= 10;

// Number of columns for categories. If you increase this, you might also want to increase the category table with in the css file
$cat_columns		= 2;

// Can speed up searches on large database (should be 0)
$bound_search_result = 0;

// The length of the description string queried when displaying search results. // If set to 0 (default), makes a query for the whole page text, // otherwise queries this many bytes. Can significantly speed up searching on very slow machines 
$length_of_link_desc	= 0;

// Number of links shown to next pages
$links_to_next		 = 9;

// Show meta description in results page if it exists, otherwise show an extract from the page text.
$show_meta_description = 1;

// Advanced query form, shows and/or buttons
$advanced_search	= 1;

// Query scores are not shown if set to 0
$show_query_scores	 = 1;	



 // Display category list
$show_categories	 = 1;

// Length of page description given in results page
$desc_length		= 250;

// Show only the 2 most relevant links from each site (a la google)
$merge_site_results		= 0;

// Enable spelling suggestions (Did you mean...)
$did_you_mean_enabled	= 1;

// Enable Sphider Suggest 
$suggest_enabled		= 1;

// Search for suggestions in query log 
$suggest_history		= 1;

// Search for suggestions in keywords 
$suggest_keywords		= 0;

// Search for suggestions in phrases 
$suggest_phrases		= 0;

// Limit number of suggestions 
$suggest_rows		= 10;


/*********************** 
Weights
***********************/

// Relative weight of a word in the title of a webpage
$title_weight  = 20;

// Relative weight of a word in the domain name
$domain_weight = 60;

// Relative weight of a word in the path name
$path_weight	= 10;

// Relative weight of a word in meta_keywords
$meta_weight	= 5;?>
