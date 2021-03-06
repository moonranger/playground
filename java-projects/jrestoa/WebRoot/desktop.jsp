<%@ page language="java" contentType="text/html; charset=UTF-8"
    pageEncoding="UTF-8"%>
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<%
	if(session.getAttribute("user") == null)
		request.getRequestDispatcher("index.html").forward(request,response);
%>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
<title>Web2.0 OA</title>
<link rel="stylesheet" type="text/css"
	href="ext2/resources/css/ext-all.css" />
<script type="text/javascript" src="ext2/adapter/ext/ext-base.js"></script>
<script type="text/javascript" src="ext2/ext-all.js"></script>
<!-- ExtJS2.0 Desktop Library -->
<script type="text/javascript" src="js/desktop/StartMenu.js"></script>
<script type="text/javascript" src="js/desktop/TaskBar.js"></script>
<script type="text/javascript" src="js/desktop/Desktop.js"></script>
<script type="text/javascript" src="js/desktop/App.js"></script>
<script type="text/javascript" src="js/desktop/Module.js"></script>
<!-- Application main JS code -->
<script type="text/javascript" src="js/main.js"></script>
<script type="text/javascript" src="js/addressbook.js"></script>
<script type="text/javascript" src="js/mail.js"></script>
<script type="text/javascript" src="js/files.js"></script>
<script type="text/javascript" src="js/calendar.js"></script>
<script type="text/javascript" src="js/todo.js"></script>
<script type="text/javascript" src="js/askoff.js"></script>
<!-- Application's main CSS styles -->
<link rel="stylesheet" type="text/css" href="css/desktop.css" />
<link rel="stylesheet" type="text/css" href="css/shortcuts.css" />
</head>
<body scroll="no">

<div id="x-desktop"><a href="dummyXML/rss.xml"
	style="margin: 10px 20px; float: right;"><img src="images/rss.png" />
</a>

<dl id="x-shortcuts">
	<dt id="win-mail-shortcut"><a href="#"><img src="images/s.gif" />
	<div>我的邮件</div>
	</a></dt>
	<dt id="win-addressbook-shortcut"><a href="#"><img
		src="images/s.gif" />
	<div>我的联系人</div>
	</a></dt>
	<dt id="win-files-shortcut"><a href="#"><img
		src="images/s.gif" />
	<div>我的文件柜</div>
	</a></dt>
</dl>
</div>

<div id="ux-taskbar">
<div id="ux-taskbar-start"></div>
<div id="ux-taskbuttons-panel"></div>
<div class="x-clear"></div>
</div>
</body>
</html>
