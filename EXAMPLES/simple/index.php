<?php
//<!DOCTYPE HTML>
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: POST, GET, OPTIONS");
header("Access-Control-Allow-Headers: X-Requested-With");
?>

<html>
<head>
 <meta charset="UTF-8">
 <title>Title of the document</title>
 <script src="jquery-1.10.2.min.js"></script>
 <script src="Eventsource.js"></script>
 <script src="jquery.eventsource.js"></script>
 <style>
 #ftext { background-color: #0af; border: 4px solid #08a; padding: 6px; border-radius: 6px; }
 .roombutton { float: left; background-color: #aaa; margin: 3px; padding: 6px; border-radius: 6px;}
 .selected { background-color: blue; color: white;}
 .sep { background-color: #08a; display: block; height: 2px; width: 100%; margin: 1px;}
 .c { clear: both; }
 input { padding: 4px; border-radius: 8px; border: 1px solid #08a; }
 input[type="text"] { width: 300px;}
 </style>
 </head>
<body>

<h1>SSE (Server Sent Events) demo page</h1>

<?php
    global $room;
    $room = '';

    // SET THIS VARIABLES !!!
    $myurl = '/index.php'; //Ulr of this php file
    //$eventprovider = '/eventprovider'; //hasses server url and matching url (this link forwarded with apache_proxy_html)
    /*direct*/  $eventprovider = 'http://sse.server.local:8080/sse-event'; //hasses server url and matching url
    /*proxied*/ //$eventprovider = '/sseeventprovider'; //hasses server url and matching url
    $ajaxhandler = '/ajaxhandler.php'; //url of ajaxhandler.php
    // END OF SET THIS VARIABLES !!!

    $clinum = rand(1,90);
    print "<div id=\"rand\">Client random number:$clinum</div><br/>";

    roomlink('first' ,"First room" ,$myurl.'?room=first' );
    roomlink('second',"Second room",$myurl.'?room=second');
    roomlink('third' ,"Third room" ,$myurl.'?room=third' );
    print "<div class=\"c\"></div><br/>";

    if($room != '')
    {
        print "You are in room:<strong>$room</strong>";

        //build subscriber parameter string
        $subs = 'common-';
        $subs .= "client_$clinum";
        $subs .= "-room_$room";

        print '<script>
                if(typeof(EventSource)!=="undefined")
                {
                     var source = 
                        new EventSource("'.$eventprovider.'?subscribe='. $subs.'");
                     source.onmessage=function(event)
                     {
                        var old;
                        old = document.getElementById("ftext").innerHTML;
                        document.getElementById("ftext").innerHTML=event.data+"<div class=\"sep\"></div>"+old;
                        //console.log("Received:"+event.data);
                    };
                }
                else
                { document.getElementById("ftext").innerHTML="Sorry, your browser does not support server-sent events..."; }
                </script>';

        print '<form id="inputform" name="input" action="'.$myurl.'" method="post">
                <input id="ustext" type="text" name="msg"/>
                <input type="submit" name="Send" value="Send"/>
                </form>
                <script>
                $("#inputform").submit(function () {
                    jQuery.ajax("'.$ajaxhandler.'?room='.$room.'&text="+jQuery("#ustext").val(),
                        function() { });
                    jQuery("#ustext").val("");
                    return false;
                });
                </script>';

        print '<div id="ftext">-- You can see chat messages here --</div>';
    }

print '</body></html>';

function roomlink($pname,$title,$link)
{
    global $room;
    if(isset($_GET['room']) && $_GET['room'] == $pname)
    {
        print "<div class=\"roombutton selected\">$title</div>";
        $room = $pname;
    }
    else
        print "<div class=\"roombutton\"><a href=\"$link\">$title</a></div>";
}
