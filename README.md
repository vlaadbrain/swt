# STILL IN PROGRESS

swt - simple widget toolkit
===========================

BNF:

<commands> ::= <command> { ";" <command> } 
<command>  ::= <window>  | <add> | <show> | <dump> | <quit> 
<window>   ::= window <sp> <class> <sp> <name> 
	returns "window <id>" 
<show>     ::= show <sp> <id> | show all 
<dump>     ::= dump <sp> all | dump <sp> <id> 
<quit>     ::= quit 
<add>      ::= add <sp> <parent> <sp> <widget> 
<parent>   ::= <id> 
<widget>   ::= <hbox> | <vbox> | <embed> 
<hbox>     ::= hbox <sp> <id> <sp> <widget-attributes> 
<vbox>     ::= vbox <sp> <id> <sp> <widget-attributes> 
<embed>    ::= embed <sp> <xid> 
<class>    ::= <string> 
<name>     ::= <id> 
<id>       ::= <string> 
<string>   ::= <letter> { <letter> | <digit> } 
<sp>       ::= " " 


Installation
------------
Edit config.mk to match your local setup. swt is installed into
/usr/local by default. 

Afterwards enter the following command to build and install swt
(if necessary as root): 

    $ make clean install 


Running swt
-----------
Simply invoke the 'swt -i <inputfile> -o <outputfile> ' command with the 
required arguments. 

you can interact with swt using both of those files and the commads above. 
The intention is that the gui is driven by potentially any language. 

THANKS ii ;) 
