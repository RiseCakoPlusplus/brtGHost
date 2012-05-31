CREATE TABLE IF NOT EXISTS `users` (
  
	`id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  
	`name` text NOT NULL,
  
	`password` text NOT NULL,
  
	`email` text NOT NULL,
  
	`lastconnect` text NOT NULL,
  
	`friendlist` text NOT NULL,
  
	 PRIMARY KEY (`id`)

) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=1;