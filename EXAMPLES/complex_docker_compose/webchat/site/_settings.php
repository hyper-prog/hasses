<?php

global $site_config;

$site_config->base_path = '';
$site_config->clean_urls = true;

$site_config->startpage_location = 'chat';

$sse_conf->sse_server_ext_host = $_SERVER['SERVER_NAME'];
$sse_conf->sse_server_int_host = 'hassesdaemon';