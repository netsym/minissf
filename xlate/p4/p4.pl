#! /usr/bin/perl

#
# p4.pl :- pretty primitive perl parser.
#
# This parser is for source-code translation using the annotations
# embedded in the SSF source code by the user. These annotations are
# embedded in the source code as the special comments preceded by the
# prefix "//!".
#

use strict;

# turn it on if one wants to pretty print the translated code
my($pretty_prt);

# context of the currently processing file
my($filename) = "";
my($outfilename) = "";
my($linenum) = 0;

# the argument list is a list of type-name pairs; At declaration, the
# names can be omitted. In this case, we supply a variable name with a
# global index number.
my($omit_argnam_idx) = 0;

# recognized wait statements: semaphore wait is not included here.
my(@wait_functions) = ('waitOn', 'waitOnFor', 'waitFor', 'waitUntil', 'waitForever',
		       'suspendForever', 'waitOnUntil');
my(%wait_functions);
foreach my $fct (@wait_functions) { $wait_functions{$fct} = 1; }

sub prep_set_lineno {
  my($filename) = shift;
  my($lineno) = shift;
  return "#line ${lineno} \"${filename}\"\n";
}

sub num_instances {
  my($word) = shift;
  my($char) = shift;
  #{ print "num_instance($word,$char)"; }

  my($cnt) = length($word);
  $word =~ s/${char}//g;
  $cnt -= length($word);
  #{ print "=>$cnt\n"; }
  return $cnt;
}

# parse function declaration, extract the following vector: (virtual,
# rtntyp, clsnam, fctnam). We assume the function declaration has the
# following format: "[virtual] rtntyp [clsnam::]fctname".
sub scan_fctdcl {
  my($thestr) = shift;
  #{ print "scan_fctdcl($thestr)"; }

  $thestr =~ s/^\s+//; $thestr =~ s/\s+$//; $thestr =~ s/\s+/ /g;

  # find the class name and the function name
  my($clsnam); my($fctnam); my(@prefix);
  if($thestr =~ m/::/) {
    # the last one (separated by ::) is the fuction name, remove it
    @prefix = split(/\s*::\s*/, $thestr);
    $fctnam = pop(@prefix);
    $thestr = join('::', @prefix);

    # the last one (separated by space) is the class name, remove it
    @prefix = split(/\s+/, $thestr);
    $clsnam = pop(@prefix);

    # to guard against the special case: int&A::a(), which does not
    # have a space to separate the class name
    if($clsnam =~ m/(.*\W)(\w+)/) {
      push(@prefix, $1);
      $clsnam = $2;
    }
  } else {
    # the last one (separated by space) contains the function name, remove it.
    # there's no class name in this case
    @prefix = split(/\s+/, $thestr);
    $clsnam = "";
    $fctnam = pop(@prefix);

    # to guard against the case: int&a(), which does not have a space
    # in-between
    if($fctnam =~ m/(.*\W)(\w+)/) {
      push(@prefix, $1);
      $fctnam = $2;
    }
  }
  $clsnam =~ s/\s//g; $fctnam =~ s/\s//g;

  # find out whether the function is virtual and what's the return type
  $thestr = join(" ", @prefix);
  my($virtual);
  if($thestr =~ /\bvirtual\b/) {
    $virtual = "virtual";
    $thestr =~ s/\s*virtual\s*/ /g;
  } else { $virtual = ""; }

  my($rtntyp) = $thestr;
  $rtntyp =~ s/^\s+//; $rtntyp =~ s/\s+$//; $rtntyp =~ s/\s+/ /g;

  #{ print "=>virt=$virtual, type=$rtntyp, cls=$clsnam, fct=$fctnam\n"; }
  return ($virtual, $rtntyp, $clsnam, $fctnam);
}

# returns the type and name from the local variable declaration, or
# argument declaration.
sub type_and_name {
  my($arg) = shift;
  #{ print "type_and_name($arg)"; }

  $arg =~ s/^\s+//; $arg =~ s/\s+$//;

  my($arg_type); my($arg_name);
  if($arg =~ m/(.*\W)(\w+)\s*(\[\d*\])*$/) {
    # this could be an array declaration (possible); make sure the
    # case "int&a" is included.
    $arg_type = $1;
    $arg_name = $2.$3;
  } else {
    # it's possible there is no variable name in argument list
    $arg_type = $arg;
    $arg_name = "_ssf_omit_${omit_argnam_idx}";
    $omit_argnam_idx++;
  }

  $arg_type =~ s/^\s+//; $arg_type =~ s/\s+$//; $arg_type =~ s/\s+/ /g;
  $arg_name =~ s/\s//g;
  #{ print "=>typ=$arg_type, nam=$arg_name\n"; }
  return ($arg_type, $arg_name)
}

# returns the position of the first wait statement in the string, if
# there's one; otherwise, -1 is returned.
sub has_wait_statement {
  my($line) = shift;
  #print STDERR" 1>$line";
  $line =~ s/\\\\/__/g;
  $line =~ s/\\\"/__/g;
  #print STDERR "2>$line";
  while($line =~ m/(\"[^"]+\")/) {
    my $str = $1;
    my $space = '_' x length($str);
    $line =~ s/\"[^"]+\"/$space/;
    #print STDERR "--->$str\n";
    #print STDERR "3>$line";
  }
  #print STDERR "4>$line";
  if($line =~ m/wait/ || $line =~ m/suspendForever/) {
    while($line =~ m/([a-zA-Z_0-9]+)/) {
      my($word) = $1;
      my($idx) = index($line, $word);
      if(defined $wait_functions{$word}) { return $idx; }
#      foreach my $wait (@wait_functions) {
#	if($word eq $wait) { return $idx; }
#      }
      my($space) = ' ' x length($word);
      $line =~ s/$word/$space/;
    }
  }
  return -1;
}

# scan through the argument list, return the array in the form (type1,
# varname1, type2, varname2, ...)
sub scan_arglist {
  my($thestr) = shift;
  #{ print "scan_arglist($thestr) =>\n"; }

  $thestr =~ s/\n/ /g;
  my(@arglist) = ();

  # if the argument list is not empty
  my $tmp = $thestr; $tmp =~ s/\s//g;
  if($tmp ne '') {
    my(@args) = split(/,/, $thestr);
    foreach my $arg (@args) {
      if($arg =~ m/\&/) {
	die "file ${filename} line ${linenum}: variable reference unsupported in argument list!";
      }
      my ($at, $an) = type_and_name($arg);
      push(@arglist, $at); push(@arglist, $an);
    }
  }
  return @arglist;
}

# returns whether the given string contains the keyword provided; if
# it does, the position of the keyword is returned; otherwise, -1 is
# returned. I forgot the reason I didn't use index() directly here.
sub has_keyword {
  my($key)  = shift;
  my($line) = shift;
  #print STDERR" 1>$line";
  $line =~ s/\\\\/__/g;
  $line =~ s/\\\"/__/g;
  #print STDERR "2>$line";
  while($line =~ m/(\"[^"]+\")/) {
    my $str = $1;
    my $space = '_' x length($str);
    $line =~ s/\"[^"]+\"/$space/;
    #print STDERR "--->$str\n";
    #print STDERR "3>$line";
  }
  #print STDERR "4>$line";

  if(index($line, $key) >= 0) {
    while($line =~ m/([a-zA-Z_0-9\$]+)/) {
      my($word) = $1;
      my($idx) = index($line, $word);
      if($word eq $key) { return $idx; }
      else {
	my($space) = ' ' x length($word);
	$line =~ s/$word/$space/;
      }
    }
  }
  return -1;
}

# analyze procedure definititon
sub define_procedure {
  my(@lines) = @_;
  my($theline) = join(//, @lines);

  # look for virtual, return type, class name, and function name
  my($openidx) = index($theline, "(");
  my($thestr) = substr($theline, 0, $openidx);
  my ($virtual, $rtntyp, $clsnam, $fctnam) = scan_fctdcl($thestr);
  if($rtntyp eq '') { $rtntyp = 'int'; } # fail proof

  # look for arguments: need to find the closing parenthesis first
  my(@charlist) = split(//, substr($theline, $openidx));
  my($closeidx) = $openidx;
  my($level) = 0;
  while(@charlist) {
    my($char) = shift @charlist;
    if($char eq '(') { $level++; }
    elsif($char eq ')') {
      $level--;
      if($level == 0) { last; }
    }
    $closeidx++;
  }
  $thestr = substr($theline, $openidx+1, $closeidx-$openidx-1);
  my(@arglist) = scan_arglist($thestr);

  # is this Process::action()?
  my($isaction) = 0;
  if($rtntyp eq 'void' && $fctnam eq 'action' && @arglist == 0) {
    $isaction = 1;
  }

  # check for local state variables
  my(@varlist) = (); my(%locals) = ();
  for(my $i=0; $i<=$#lines; $i++) {
    $thestr = $lines[$i];
    if($thestr =~ m/\/\s*\/\s*\!\s+SSF\s+STATE/) {
      my($rest);
      ($thestr, $rest) = split(/\/\s*\/\s*\!/, $thestr);
      $thestr =~ s/\;\s*$//;
      my ($arg_type, $arg_name) = type_and_name($thestr);
      push(@varlist, $arg_type);
      push(@varlist, $arg_name);
      if($arg_name =~ /_ssf_omit_/) {
	# we can't allow variable name missing here
	die "file ${filename} line ${linenum}: unrecognized state variable declaration\n";
      }
      #if($arg_name =~ /(\w+)/) { $locals{$1} = 1; } <= don't know why!
      if(defined $locals{$arg_name}) {
	die "file ${filename} line ${linenum}: duplicate local variable ${arg_name}";
      }
      $locals{$arg_name} = 1;
      $lines[$i] = "\n"; # rid of the line
    }
  }

  # arguments are among things to be changed
  my(@al) = @arglist;
  while(@al) {
    shift @al; my($var) = shift @al;
    #if($var =~ /(\w+)/) { $locals{$1} = 1; } <= don't know why!
    if(defined $locals{$var}) {
      die "file ${filename} line ${linenum}: argument ${var} overshadows a local variable of the same name";
    }
    $locals{$var} = 1;
  }

  # add the final return statement: the last line must contain
  # '}'. The transformed function will be a function of void
  # return-type.
  $openidx = rindex($lines[$#lines], '}');
  substr($lines[$#lines], $openidx, 1) = "return;";
  push(@lines, "}\n");
  $theline = join('', @lines);

  # count the number of break points
  my($nlabels) = 0;
  foreach $thestr (@lines) {
    if($thestr =~ m/\/\s*\/\s*\!\s+SSF\s+CALL/) { $nlabels++; }
    else {
      my($pos);
      my($str) = $thestr;
      while(($pos=has_wait_statement($str)) >= 0) {
	$nlabels++;
	$str = substr($str, $pos+4); # pass the current wait statement (the first 4 letters)
      }
      $str = $thestr; # start over
      while(($pos=has_keyword('wait', $str)) >= 0) { # semaphore wait
	$nlabels++;
	$str = substr($str, $pos+4);
      }
    }
  }
  if($nlabels == 0) {
    print STDERR "WARNING: file ${filename} line ${linenum}: no suspension found in ";
    print STDERR "procedure ${fctnam}(), will cause infinite loop!\n";
  }

  # output the procedure class for this function
  if($pretty_prt) {
    print OUTFILE "//------------------------ EMBEDDED CODE STARTS ----------------------------//\n";
    print OUTFILE "// define the procedure class for ${fctnam}\n";
  }
  print OUTFILE "class _ssf_procedure_${clsnam}_${fctnam} : public minissf::Procedure \{ ";
  if($pretty_prt) { print OUTFILE "\n "; }
  print OUTFILE "public: ";
  if($pretty_prt) { print OUTFILE "\n  "; }
  my(@list) = @arglist;
  while(@list) {
    my($arg_type) = shift @list;
    my($arg_name) = shift @list;
    print OUTFILE "${arg_type} ${arg_name}; ";
    if($pretty_prt) { print OUTFILE "// function argument\n  "; }
  }
  @list = @varlist;
  while(@list) {
    my($var_type) = shift @list;
    my($var_name) = shift @list;
    print OUTFILE "${var_type} ${var_name}; ";
    if($pretty_prt) { print OUTFILE "// local variable\n  "; }
  }
  if($isaction) {
    print OUTFILE "_ssf_procedure_${clsnam}_${fctnam}() : ";
    print OUTFILE "minissf::Procedure(0, 0) {} ";
    if($pretty_prt) { print OUTFILE "\n"; }
  } else {
    print OUTFILE "_ssf_procedure_${clsnam}_${fctnam}(minissf::ProcedureContainer* _ssf_focus, ";
    @list = @arglist;
    while(@list) {
      my($arg_type) = shift @list;
      my($arg_name) = shift @list;
      print OUTFILE "${arg_type} _ssf_local_${arg_name}, ";
    }
    print OUTFILE "void* _ssf_retaddr) : ";
    if($pretty_prt) { print OUTFILE "\n    "; }
    print OUTFILE "minissf::Procedure((minissf::ProcedureFunction)";
    if($clsnam ne '') { print OUTFILE "&${clsnam}::${fctnam}, "; }
    else {
      print STDERR "WARNING: definition of SSF procedures inside class declaration is discouraged!\n";
      print STDERR "         GNU compiler may complain about this translation!\n";
      print OUTFILE "&${fctnam}, ";
    }
    if($rtntyp ne 'void') {  print OUTFILE "_ssf_focus, _ssf_retaddr, sizeof(${rtntyp}))"; }
    else { print OUTFILE "_ssf_focus, _ssf_retaddr, 0)"; }
    @list = @arglist;
    while(@list) {
      my($arg_type) = shift @list;
      my($arg_name) = shift @list;
      if($arg_name =~ /(\w+)/)  { # <= don't know why!
	print OUTFILE ", $1(_ssf_local_$1)";
      }
    }
    print OUTFILE " {} ";
    if($pretty_prt) { print OUTFILE "\n"; }
  }
  print OUTFILE "}; ";
  if($pretty_prt) { print OUTFILE "\n"; }

  # output _ssf_create_procedure_??? function
  if($pretty_prt) { print OUTFILE "// the _ssf_create_procedure_* method is generated for each procedure\n"; }
  if($clsnam ne '') {
    print OUTFILE "minissf::Procedure* ${clsnam}::_ssf_create_procedure_${fctnam}(";
  } else { print OUTFILE "minissf::Procedure* _ssf_create_procedure_${fctnam}("; }
  if($isaction) {
    print OUTFILE ") { ";
    if($pretty_prt) { print OUTFILE "\n  "; }
    print OUTFILE "return new _ssf_procedure_${clsnam}_action(); ";
  } else {
    @list = @arglist;
    while(@list) {
      my($arg_type) = shift @list;
      my($arg_name) = shift @list;
      print OUTFILE "${arg_type} ${arg_name}, ";
    }
    if($clsnam eq '') { print OUTFILE "void* retaddr = 0) { "; }
    else { print OUTFILE "void* retaddr) { "; }
    if($pretty_prt) { print OUTFILE "\n  "; }
    print OUTFILE "return new _ssf_procedure_${clsnam}_${fctnam}(this, ";
    @list = @arglist;
    while(@list) {
      my($arg_type) = shift @list;
      my($arg_name) = shift @list;
      if($arg_name =~ /(\w+)/) { # <= don't know why!
	print OUTFILE "$1, ";
      }
    }
    print OUTFILE "retaddr); ";
  }
  if($pretty_prt) { print OUTFILE "\n"; }
  print OUTFILE "\}\n";

  # output the body of the function
  if($pretty_prt) { print OUTFILE "// function body is transformed\n"; }
  if($clsnam ne '') { print OUTFILE "void ${clsnam}::${fctnam}"; }
  else { print OUTFILE "void ${fctnam}"; }
  if($isaction) { print OUTFILE "() { "; }
  else { print OUTFILE "(minissf::Process* _ssf_proc) { "; }
  if($pretty_prt) { print OUTFILE "\n  "; }
  if($isaction) {
    print OUTFILE "minissf::Process* _ssf_proc = this; ";
    if($pretty_prt) { print OUTFILE "\n  "; }
  }

  # Check whether this function qualifies for being a starting
  # procedure, which must contain only one argument and the argument
  # must be a pointer to the Process class. A starting procedure is
  # responsible for putting the first procedure frame onto the stack.
  if($isaction || scalar(@arglist) == 2 &&
      $arglist[0] =~ m/^\s*Process\s*\*\s*/) {
    print OUTFILE "if(!_ssf_proc->top_stack()) { ";
    if($pretty_prt) { print OUTFILE "\n    "; }
    print OUTFILE "_ssf_proc->push_stack(_ssf_create_procedure_${fctnam}";
    if($isaction) { print OUTFILE "()); "; }
    else { print OUTFILE "(_ssf_proc)); "; }
  } else {
    print OUTFILE "if(!_ssf_proc->top_stack()) { ";
    if($pretty_prt) { print OUTFILE "\n    "; }
    print OUTFILE "minissf::ThrowableStream(\"${filename}\", ${linenum}, \"${fctnam}\") << \"not a starting procedure\"; ";
  }
  if($pretty_prt) { print OUTFILE "\n  "; }
  print OUTFILE "\} ";
  if($pretty_prt) { print OUTFILE "\n  "; }
  print OUTFILE "if(!_ssf_proc || !_ssf_proc->in_procedure_context()) minissf::ThrowableStream(\"${filename}\", ${linenum}, \"${fctnam}\") << \"improper procedure call\"; ";
  print OUTFILE "// PROPER_PROCEDURE(_ssf_proc); ";
  if($pretty_prt) { print OUTFILE "\n  "; }

  # _ssf_pframe is the current procedure frame on stack
  print OUTFILE "_ssf_procedure_${clsnam}_${fctnam}* _ssf_pframe = ";
  print OUTFILE "(_ssf_procedure_${clsnam}_${fctnam}*)_ssf_proc->top_stack(); ";
  if($pretty_prt) { print OUTFILE "\n  "; }

  # jump table
  print OUTFILE "switch(_ssf_pframe->entry_point) { ";
  if($pretty_prt) { print OUTFILE "\n  "; }
  my($label);
  for($label=1; $label<=$nlabels; $label++) {
    if($pretty_prt) { print OUTFILE "  "; }
    print OUTFILE "case ${label}: goto _ssf_sync${label}; ";
    if($pretty_prt) { print OUTFILE "\n  "; }
  }
  print OUTFILE "\}\n";
  if($pretty_prt) { print OUTFILE "\n  "; }

  # correct the line number at the first statement of the function
  $theline =~ m/([^{]*\{\s*)/;
  my($prefix) = $1;
  $theline = substr($theline, length($prefix));
  $linenum += ($prefix =~ tr/\n/\n/);
  print OUTFILE prep_set_lineno($filename, $linenum);
  @lines = split(/\n/, $theline);

  $label = 1;
  my($offset);
  my($notinquote) = 1;
  for($offset=0; $offset<=$#lines; $offset++, $linenum++) {
    $thestr = $lines[$offset];
	
    # replace state variables

    # We don't want the field names in a data structure to be changed
    # and we don't want anything in single quotes to be changed as
    # well. We hereby split the line with them.
    my(@words) = split(/(\.\s*[a-zA-Z0-9_]+|->\s*[a-zA-Z0-9_]+|[a-zA-Z0-9_]+|'[\\]?[a-zA-Z0-9_]')/, $thestr);
    my(@newwords) = ();
    foreach my $word (@words) {
      # get the number of instances a word contains " but not \"
      # Strangely, since we use s// operator in num_instances()
      # function, \" must survive string evaluation twice! Don't ask
      # me why. Divide by two since the character is actually two
      # letters long.
      my($ninsts) = num_instances($word, '"')-num_instances($word, '\\\\"')/2;
      if($ninsts&1 == 1) { # is odd
	if($notinquote) { $notinquote = 0; } else { $notinquote = 1; }
	push(@newwords, $word);
      } elsif(defined($locals{$word}) && $notinquote) {
	push(@newwords, "_ssf_pframe->$word");
      }
      elsif($word eq "return" && $notinquote) { push(@newwords, "\$R\$E\$T"); }
      else { push(@newwords, $word); }
    }
    $thestr = join('', @newwords);

    # transform return statement
    my($open_idx) = has_keyword('$R$E$T', $thestr);
    if($open_idx >= 0) {
      # there could be multiple returns in one line; we process them one by one
      my($zzz) = '';
      while($open_idx >= 0) {
	$zzz .= substr($thestr, 0, $open_idx);
	my($close_idx) = index($thestr, ";", $open_idx);
	if($close_idx < 0) {
	  die "file ${filename} line ${linenum}: return statement across multiple lines!";
	}
	my($ret) = substr($thestr, $open_idx+6, $close_idx-$open_idx-6);

	my($filt) = $ret;
	$filt =~ s/\s//g;
	if($filt ne '') {
	  if($rtntyp eq 'void') {
	    die "file ${filename} line ${linenum}: return value in void function!";
	  }
	  $zzz .= "{ ${rtntyp} _ssf_tmp = ${ret}; _ssf_pframe->call_return(&_ssf_tmp); return; }";
	} else {
	  $zzz .= "{ _ssf_pframe->call_return(); return; }";
	}
	$thestr = substr($thestr, $close_idx+1);
	$open_idx = has_keyword('$R$E$T', $thestr);
      }
      $thestr = $zzz.$thestr;
    }

    # transform wait statement
    $open_idx = has_wait_statement($thestr);
    if($open_idx >= 0) {
      # there could be several wait statements; we transform them one
      # by one
      my($zzz) = '';
      while($open_idx >= 0) {
	# preidx indicate where the wait statement starts
	my($preidx) = rindex($thestr, ';', $open_idx);
	my($tmp) = rindex($thestr, '}', $open_idx);
	if($preidx < $tmp) { $preidx = $tmp; }
	$tmp = rindex($thestr, '{', $open_idx);
	if($preidx < $tmp) { $preidx = $tmp; }
	$tmp = rindex($thestr, 'else', $open_idx);
	if($preidx < $tmp) { $preidx = $tmp+length('else')-1; }
	$tmp = rindex($thestr, ')', $open_idx);
	while($tmp > $preidx) {
	  # to avoid cases like: (&a)->waitFor
	  my($refstr) = substr($thestr, $tmp+1, $open_idx-$tmp-1);
	  $refstr =~ s/\s//g;
	  if(substr($refstr, 0, 2) eq '->' || substr($refstr, 0, 1) eq '.') {
	    $tmp = rindex($thestr, ')', $tmp-1);
	  } else { last; }
	}
	if($preidx < $tmp) { $preidx = $tmp; }
	
	if($preidx >= 0) { $preidx++; } else { $preidx = 0; }
	my($postidx) = index($thestr, ';', $open_idx);
	if($postidx < 0) {
	  die "file ${filename} line ${linenum}: can't handle wait statement across multiple lines!\n";
	}
	$zzz .= substr($thestr, 0, $preidx);

	my($prefix) = substr($thestr, $preidx, $open_idx-$preidx);
	if($prefix =~ m/=.*=/) {
	  die "file ${filename} line ${linenum}: confusing wait statement!\n";
	}
	my($lhs); my($refs);
	if($prefix =~ m/=/) {
	  ($lhs, $refs) = split(/=/, $prefix);
	} else {
	  $lhs = '';
	  $refs = $prefix;
	}
		
	my($postfix) = substr($thestr, $open_idx, $postidx-$open_idx);
	$open_idx = index($postfix, '(');
	if($open_idx < 0) {
	  die "file ${filename} line ${linenum}: confusing wait statement!\n";
	}
	#$close_idx = rindex($postfix, ")");
	my(@charlist) = split(//, substr($postfix, $open_idx));
	my $close_idx = $open_idx;
	my($level) = 0;
	while(@charlist) {
	  my($char) = shift @charlist;
	  if($char eq '(') { $level++; }
	  elsif($char eq ')') {
	    $level--;
	    if($level eq 0) { last; }
	  }
	  $close_idx++;
	}
	if($level > 0) {
	  die "file ${filename} line ${linenum}: confusing wait statement!\n";
	}
	my($wait) = substr($postfix, 0, $open_idx);
	my($args) = substr($postfix, $open_idx+1, $close_idx-$open_idx-1);
	
	$zzz .= "{ _ssf_pframe->entry_point = ${label}; ";
	$zzz .= "${refs}${wait}(${args}); ";
	$zzz .= "if(_ssf_pframe->call_suspended()) { return; _ssf_sync${label}: ; ";
	if($lhs ne '') {
	  $zzz .= "${lhs} = ${refs}timed_out(); ";
	}
	$zzz .= "} }";
	$label++;

	$thestr = substr($thestr, $postidx+1);
	$open_idx = has_wait_statement($thestr);
      }
      $thestr = $zzz.$thestr;
    }

    # transform semWait statement
    $open_idx = has_keyword('wait', $thestr); # semaphore wait
    if($open_idx >= 0) {
      my($zzz) = '';
      while($open_idx >= 0) {
	my($preidx) = rindex($thestr, ';', $open_idx);
	my($tmp) = rindex($thestr, '}', $open_idx);
	if($preidx < $tmp) { $preidx = $tmp; }
	$tmp = rindex($thestr, '{', $open_idx);
	if($preidx < $tmp) { $preidx = $tmp; }
	$tmp = rindex($thestr, 'else', $open_idx);
	if($preidx < $tmp) { $preidx = $tmp+length('else')-1; }
	$tmp = rindex($thestr, ')', $open_idx);
	while($tmp > $preidx) {
	  my($refstr) = substr($thestr, $tmp+1, $open_idx-$tmp-1);
	  $refstr =~ s/\s//g;
	  if(substr($refstr, 0, 2) eq '->' || substr($refstr, 0, 1) eq '.') {
	    $tmp = rindex($thestr, ')', $tmp-1);
	  } else { last; }
	}
	if($preidx < $tmp) { $preidx = $tmp; }

	if($preidx >= 0) { $preidx++; } else { $preidx = 0; }
	my($postidx) = index($thestr, ';', $open_idx);
	if($postidx < 0) {
	  die "file ${filename} line ${linenum}: can't handle wait statement across multiple lines!\n";
	}
	$zzz .= substr($thestr, 0, $preidx);

	my($prefix) = substr($thestr, $preidx, $open_idx-$preidx);

	my($postfix) = substr($thestr, $open_idx, $postidx-$open_idx);
	$open_idx = index($postfix, "(");
	#$close_idx = rindex($postfix, ")");
	my(@charlist) = split(//, substr($postfix, $open_idx));
	my $close_idx = $open_idx;
	my($level) = 0;
	while(@charlist) {
	  my($char) = shift @charlist;
	  if($char eq '(') { $level++; }
	  elsif($char eq ')') {
	    $level--;
	    if($level == 0) { last; }
	  }
	  $close_idx++;
	}
	if($level > 0) {
	  die "file ${filename} line ${linenum}: confusing wait statement!\n";
	}
	my($args) = substr($postfix, $open_idx+1, $close_idx-$open_idx-1);
	$args =~ s/\s//g;
	if($args ne '') {
	  die "file ${filename} line ${linenum}: semaphore wait does not have argument\n";
	}

	$zzz .= "{ if(${prefix}value()>0) ${prefix}wait(); ";
	$zzz .= "else { _ssf_pframe->entry_point = ${label}; ";
	$zzz .= "${prefix}wait(); return; ";
	$zzz .= "_ssf_sync${label}: ; } }";
	$label++;

	$thestr = substr($thestr, $postidx+1);
	$open_idx = has_keyword('wait', $thestr);
      }
      $thestr = $zzz.$thestr;
    }

    # transform procedure calls
    if($thestr =~ m/\/\s*\/\s*\!\s+SSF\s+CALL/) {
      $thestr =~ s/\/\s*\/\s*\!\s+SSF\s+CALL//;
      $thestr =~ s/\s//g;
      if($thestr ne '') {
	die "file ${filename} line ${linenum}: '//! SSF CALL' must itself occupy a line above the procedure call!\n";
      }
      print OUTFILE "\n";
      $offset++; $linenum++;
      $thestr = $lines[$offset];

      # transform state variables
      my(@words) = split(/(\.\s*[a-zA-Z0-9_]+|->\s*[a-zA-Z0-9_]+|[a-zA-Z0-9_]+)/, $thestr);
      my(@newwords) = ();
      foreach my $word (@words) {
	if(defined($locals{$word})) { push(@newwords, "_ssf_pframe->$word"); }
	else { push(@newwords, $word); }
      }
      $thestr = join('', @newwords);

      if($thestr !~ m/;\s*$/ || $thestr =~ m/;.*;/) {
	die "file ${filename} line ${linenum}: can't handle procedure call in multiple lines!\n";
      }
      $thestr =~ s/;//;
      if($thestr =~ m/=.*=/) {
	die "file ${filename} line ${linenum}: confusing procedure call!\n";
      }
      my($lhs); my($rhs);
      if($thestr =~ m/=/) {
	($lhs, $rhs) = split(/=/, $thestr);
      } else {
	$lhs = '';
	$rhs = $thestr;
      }

      my(@rhslist) = split(//, $rhs);
      my($pos) = $#rhslist;
      while($pos && $rhslist[$pos] ne ')') {
	$pos--;
      }
      if(!$pos) {
	die "file ${filename} line ${linenum}: confusing procedure call!\n";
      }
      $pos--;
      my($plevel) = 1;
      my($arglen) = 0;
      while($pos > -1 && $plevel) {
	if($rhslist[$pos] eq ')') { $plevel++; }
	elsif($rhslist[$pos] eq '(') { $plevel--; }
	if($plevel) { $arglen++; $pos--; }
      }
      if($pos < 0) {
	die "file ${filename} line ${linenum}: confusing procedure call!\n";
      }
      my($args) = substr($rhs, $pos+1, $arglen);
      my($args_clobber) = $args;
      $args_clobber =~ s/\s//g;

      $pos--; my($namlen) = 0;
      while($pos > -1) {
	if($rhslist[$pos] =~ m/[A-Za-z_0-9]/) { $namlen++; }
	else { last; }
	$pos--;
      }
      my($fctnam) = substr($rhs, $pos+1, $namlen);
      my($refs) = substr($rhs, 0, $pos+1);

      $thestr  = "{ _ssf_pframe->entry_point = ${label}; ";
      if($lhs ne '') {
	if($args_clobber ne '') {
	  $thestr .= "_ssf_pframe->call_procedure(${refs}_ssf_create_procedure_${fctnam}(${args}, &${lhs})); ";
	} else {
	  $thestr .= "_ssf_pframe->call_procedure(${refs}_ssf_create_procedure_${fctnam}(&${lhs})); ";
	}
      } else {
	$thestr .= "_ssf_pframe->call_procedure(${refs}_ssf_create_procedure_${fctnam}(${args})); ";
      }
      $thestr .= "if(_ssf_pframe->call_suspended()) { return; _ssf_sync${label}: ; } }";
      $label++;
    }

    print OUTFILE "$thestr\n";
  }
  if($pretty_prt) {
    print OUTFILE "//------------------------ EMBEDDED CODE ENDS ----------------------------//\n";
  }
}

# transform procedure declaration
sub declare_procedure {
  my($theline) = shift;

  # look for virtual, return type, class name, and function name
  my($openidx) = index($theline, "(");
  my($thestr) = substr($theline, 0, $openidx);
  my ($virtual, $rtntyp, $clsnam, $fctnam) = scan_fctdcl($thestr);
  if($clsnam ne '') {
    die "file $filename line $linenum: unrecognized procedure declaration!\n";
  }
  if($rtntyp eq '') {
    print STDERR "WARNING: can't resolve return type for function ${clsnam}::${fctnam},\n";
    print STDERR "         treat as an int type.\n";
    $rtntyp = 'int';
  }

  # look for the matching close parenthesis
  my($closeidx);
  my($v) = $openidx; my($match) = 1;
  while($match > 0) {
    $closeidx = $v+1;
    my($x) = index($theline, '(', $closeidx);
    my($y) = index($theline, ')', $closeidx);
    if($x < 0) { $v = $y; }
    elsif($y < 0) { $v = $x; }
    elsif($x < $y) { $v = $x; }
    else { $v = $y; }
    if($v < 0) { die "file $filename line $linenum: unrecognized procedure declaration!\n"; }
    if($v eq $x) { $match++; } else { $match--; }
  }
  $closeidx = $v;

  # look for arguments
  #my($closeidx) = rindex($theline, ')');
  #if($closeidx < 0) { die "file $filename line $linenum: unrecognized procedure declaration!\n"; }
  $thestr = substr($theline, $openidx+1, $closeidx-$openidx-1);
  my($reststr) = substr($theline, $closeidx+1);
  my(@arglist) = scan_arglist($thestr);

  # is this SSF_Process::action?
  my($isaction) = 0;
  if($rtntyp eq 'void' && $fctnam eq 'action' && scalar(@arglist) == 0) {
    $isaction = 1;
  }

  # output __create_procedure__??? function
  if($pretty_prt) {
    print OUTFILE "//------------------------ EMBEDDED CODE STARTS ----------------------------//\n";
  }
  if($virtual ne '') { print OUTFILE "virtual "; }
  print OUTFILE "minissf::Procedure* _ssf_create_procedure_${fctnam}(";
  if($isaction) { print OUTFILE "); "; }
  else {
    my @list = @arglist;
    while(@list) {
      my($arg_type) = shift @list;
      my($arg_name) = shift @list;
      print OUTFILE "${arg_type} ${arg_name}, ";
    }
    print OUTFILE "void* retaddr = 0);";
  }
  if($pretty_prt) { print OUTFILE "\n"; }

  # output the function declaration
  if($isaction) {
    if($virtual ne '') { print OUTFILE "virtual "; }
    print OUTFILE "void action()${reststr}\n";
  } else {
    print OUTFILE "void ${fctnam}(minissf::Process*)";
    my($newreststr) = $reststr;
    $newreststr =~ s/\s//g;
    if($newreststr eq "=0;") {
      die "file ${filename} line ${linenum}: abstract method unsupported!";
    } else {
      #if($reststr ne ";") { die "file ${filename} line ${linenum}: unrecognized procedure declaration!"; }
      print OUTFILE "${reststr}\n";
    }
  }
  if($pretty_prt) {
    print OUTFILE "//------------------------ EMBEDDED CODE ENDS ----------------------------//\n";
  }
}

#################################################################################
# main control starts here.
#################################################################################

if(scalar(@ARGV) == 2) {
  $pretty_prt = 0; $filename = shift @ARGV; $outfilename = shift @ARGV;
} elsif(scalar(@ARGV) == 3) {
  my $myarg = shift @ARGV;
  if($myarg eq '-x') {
    $pretty_prt = 1; $filename = shift @ARGV; $outfilename = shift @ARGV;
  } else { die "Usage: $0 [-x] filename outfilename\n"; }
} else { die "Usage: $0 [-x] filename outfilename\n"; }

die "Can't open $filename\n" unless open(SSFFILE, "$filename");
die "Can't open $outfilename\n" unless open(OUTFILE, ">$outfilename");

#print OUTFILE prep_set_lineno($filename, 1);
#print OUTFILE "#include \"ssf.h\"\n";
my $line_number = 0;
while(!eof(SSFFILE)) {
  # read in one line of source code
  my $one_line = <SSFFILE>; $line_number++;
  if($one_line =~ m/^#\s+(\d+)\s+"([^"]+)"/ ||
     $one_line =~ m/^#line\s+(\d+)\s+"([^"]+)"/) {
    my $L = $1;
    my $F = $2;
    if($F =~ m/_tmp[0-9]+_.cc/) {
      # this is the reference to the temperary file, which was
      # generated by the preprocessor!  we don't need it!
      print OUTFILE "\n";
    } else {
      $line_number = $L;
      $filename = $F;
      print OUTFILE $one_line; # the line may contain extra information
      #print OUTFILE prep_set_lineno($filename, $line_number);
      $line_number--;
    }
    next;
  }
  #elsif($one_line =~ m/^#pragma/) {
  #  print OUTFILE "\n";
  #  next;
  #}

  $one_line =~ s/\/\s*\/\s*\!/\/\/\!/;
  my $inst_pos = index($one_line, "\/\/\!");
  if($inst_pos >= 0) { # if the line contains instrumentation
    #$one_line =~ s/\/\/\!/\/\/\!/;
    my $inst_mark = substr($one_line, $inst_pos);
    $inst_mark =~ tr/A-Za-z/ /cs; # squeeze space
    if($inst_mark =~ m/SSF PROCEDURE/) {
      # //! SSF PROCEDURE
      my $inst_str = substr($one_line, 0, $inst_pos);

      my $filt = $inst_str;
      $filt =~ s/\s//g;
      if($filt eq '') {
	# this is a procedure definition: one line above the real
	# procedure call
	my $proc_lineno = $line_number+1;
	my @lines = ();
	my $level = 0;
	my $theline;
      collect:
	# get the entire procedure definition
	while($theline = <SSFFILE>) {
	  $line_number++;
	  if($theline =~ m/^#\s+(\d+)\s+"([^"]+)"/ ||
	     $theline =~ m/^#line\s+(\d+)\s+"([^"]+)"/) {
	    # this line is a compiler directive
	    my $L = $1;
	    my $F = $2;
	    if($F =~ m/_tmp[0-9]+_.cc/) {
	      $theline = "\n";
	    } else {
	      $line_number = $L-1;
	      $filename = $F;
	    }
	  }
	  elsif($theline =~ m/\{/ || $theline =~ m/\}/) {
	    my @charlist = split(//, $theline);
	    my $idx = 0;
	    while(@charlist) {
	      my $char = shift(@charlist); $idx++;
	      if($char eq "{") { $level++; }
	      elsif($char eq "}") {
		$level--;
		if($level eq 0) {
		  push(@lines, (substr($theline, 0, $idx-1)."\n"));
		  $filt = join('', @charlist);
		  $filt =~ s/\s//g;
		  if($filt ne '') { $theline = substr($theline, $idx); }
		  else { $theline = ''; }
		  last collect;
		}
	      }
	    }
	  }
	  push(@lines, $theline);
	}
	if(eof(SSFFILE) && $level != 0) { die "Unexpected end-of-file\n"; }

	# set the line number to the beginning of the procedure
	$linenum = $proc_lineno;
	define_procedure(@lines);

	# set the line number to the end of the procedure after translation
	if($theline ne '') {
	  print OUTFILE prep_set_lineno($filename, $line_number);
	  print OUTFILE $theline;
	}
	my $lno = $line_number+1;
	print OUTFILE prep_set_lineno($filename, $lno);
      }

      else { # this is a procedure declaration
	$linenum = $line_number;
	declare_procedure($inst_str);
	my $lno = $line_number+1;
	print OUTFILE prep_set_lineno($filename, $lno);
      }
    } else { print OUTFILE $one_line; }
  } else { print OUTFILE $one_line; }
}

close SSFFILE;
close OUTFILE;
