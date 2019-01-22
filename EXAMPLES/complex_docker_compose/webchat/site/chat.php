<?php

function hook_chat_introducer()
{
    return ['chat' => l('Chatpage','chat')];
}

function hook_chat_boot()
{
    global $sse_conf;

    $sse_conf = new StdClass();
    $sse_conf->sse_server_ext_host = '';
    $sse_conf->sse_server_int_host = '';
    $sse_conf->sse_server_comm_port = '8085';
    $sse_conf->sse_server_sse_port  = '8080';
    $sse_conf->sse_server_sse_path  = '/sse';
}

function hook_chat_defineroute()
{
    return [
      ['path' => 'chat','callback' => 'pc_chatpage'],
      ['path' => 'sendajax','callback' => 'aj_sendajax','type'=>'ajax'],
    ];
}

function pc_chatpage()
{
    global $sse_conf;

    ob_start();
    print "<h2>Chat through SSE</h2>";
    print "This page does not use database, the chat will disapper on page reload.<br/>";
    $f = new HtmlForm('sd');
    $f->action_ajax('sendajax');
    $f->input('text','text','',['id' => 'tosend','size'=>40]);
    $f->input('submit','sendbtn','Send');
    print $f->get();

    $sseurl = 'http://'.$sse_conf->sse_server_ext_host.':'.$sse_conf->sse_server_sse_port.$sse_conf->sse_server_sse_path;
    $sselink = url($sseurl,['subscribe' => 'room'.rand(100,999)]);
    add_style('.chatout { padding: 5px; margin: 5px; border: 2px solid #ffaa66; background-color: #454545; color: #efefef; }');
    print '<div id="ftext" class="chatout"></div>';
    print '<script>
            if(typeof(EventSource)!=="undefined")
            {
                var source = new EventSource("'.$sselink.'");
                source.onmessage=function(event)
                {
                    var old;
                    old = document.getElementById("ftext").innerHTML;
                    document.getElementById("ftext").innerHTML=event.data+"<hr/>"+old;
                    console.log("Received:"+event.data);
                };
            }
            else
            {
                document.getElementById("ftext").innerHTML="Sorry, your browser does not support server-sent events..."; 
            }
           </script>';

    return ob_get_clean();
}

function aj_sendajax()
{
    par_def('sendbtn','text0');
    par_def('text','text4m');
    if(par_is('sendbtn','Send'))
    {
        form_source_check();
        send_sse_message_throughnet('*',par('text'));
        ajax_add_val('#tosend','');
    }
}

function send_sse_message_throughnet($token,$message)
{
    global $sse_conf;
    $sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    if(socket_connect($sock,$sse_conf->sse_server_int_host,$sse_conf->sse_server_comm_port))
    {
        $sdata = $token.'='.$message;
        socket_send($sock,$sdata,strlen($sdata),MSG_EOR);
        socket_shutdown($sock,1);
        socket_close($sock);
    }
}

