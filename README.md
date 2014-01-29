swt - simple widget toolkit
===========================

Installation
------------
Edit config.mk to match your local setup. swt is installed into  
/usr/local by default.   

Afterwards enter the following command to build and install swt  
(if necessary as root): 

    make install 


Running swt
-----------
Simply invoke  

	swt -i <inputfile> -o <outputfile>

you can interact with swt using both of those files and the commads below.  
The intention is that the gui is driven by potentially any language.  

Commands
--------

	BNF:

	<commands> ::= <command> { ";" <command> }  
	<command>  ::= <window>  | <add> | <show> | <dump> | <quit>  
	<hwindow>   ::= window <sp> <name> <title>  
		returns "window <name> <xid>"  
	<hwindow>   ::= hwindow <sp> <name> <title>  
	<vwindow>   ::= vwindow <sp> <name> <title>  
	<show>     ::= show <sp> <name> | show all  
	<dump>     ::= dump <sp> all | dump <sp> <name>  
	<quit>     ::= quit  
	<add>      ::= add <sp> <parent> <sp> <widget>  
	<parent>   ::= <name>  
	<widget>   ::= <text> | <embed>  
	<text>     ::= text <sp> <name> <sp> <widget-attributes>  
	<embed>    ::= embed <sp> <xid>  
	...
	<name>     ::= <alpha-num>  
	<xid>      ::= <unsignedlong>  
	<alpha-num>::= <letter> | <digit> { <letter> | <digit> }  
	<sp>       ::= " "  

Customizations
--------------
(cp config.def.h config.h || make) && $EDITOR config.h && make;

Like most simple projects swt doesn't provide some fancy interface  
to *theme* the widget toolkit.  The best interface for that is your  
favorite text editor pointed at config.h; and recompile...  

Tests
-----
currently tests will require xdotool to automate X events
- window.sh is a crude attempt at creating windows and quiting

Gratitudes
----------
THANKS ii,tabbed,swk ;)
