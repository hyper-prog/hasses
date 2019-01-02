<?php
header('Content-Type: text/event-stream');
header('Cache-Control: no-cache'); // recommended to prevent caching of event data.

global $fifo_file;
$fifo_file = "/var/run/hasses/SSE_EVENT";

send_sse_message('room_'.$_GET['room'],$_GET['text']);

function send_sse_message($token,$message)
{
	global $fifo_file;
	file_put_contents($fifo_file,$token.'='.$message);
}

?>
