<?php
header('Content-Type: text/event-stream');
header('Cache-Control: no-cache'); // recommended to prevent caching of event data.

global $fifo_file;
$fifo_file = "/var/run/hasses/SSE_EVENT";

global $commchannel_host;
global $commchannel_port;

$commchannel_host = '127.0.0.1';
$commchannel_port = 8085;

function send_sse_message_throughfifo($token,$message)
{
    global $fifo_file;
    file_put_contents($fifo_file,$token.'='.$message);
}

function send_sse_message_throughnet($token,$message)
{
    global $commchannel_host;
    global $commchannel_port;
    $sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    if(socket_connect($sock,$commchannel_host,$commchannel_port))
    {
        $sdata = $token.'='.$message;
        socket_send($sock,$sdata,strlen($sdata),MSG_EOR);
        socket_shutdown($sock,1);
        socket_close($sock);
    }
}


send_sse_message_throughnet('room_'.$_GET['room'],$_GET['text']);

?>
