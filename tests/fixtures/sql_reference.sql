-- Golden case derived from original sql_importer.py behavior
CREATE TABLE `people` (`id` int,`name` varchar(80),`score` double,`enabled` bool,PRIMARY KEY (`id`));
INSERT INTO `people` VALUES (1,'Ada, Lovelace',2.5,TRUE),(2,'O\'Brien',NULL,FALSE);
INSERT INTO `people` (`id`,`name`) VALUES (3,'Mira');
# ignored
CREATE TABLE `empty` (`id` int);
