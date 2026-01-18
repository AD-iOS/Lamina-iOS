#!/usr/bin/env perl
use strict;

die "config.ini not found" unless -f "config.ini";
die "version.hpp.in not found" unless -f "version.hpp.in";
die "help_text.hpp.in not found" unless -f "help_text.hpp.in";

open my $cfg, '<', 'config.ini' or die $!;
my ($major, $minor, $patch, $version, @help_text);
my $in_help = 0;

while (<$cfg>) {
    if (/^HELP_TEXT\s*=\s*\{/) {
        $in_help = 1;
        next;
    }

    if ($in_help && /^\}/) {
        $in_help = 0;
        next;
    }

    if ($in_help) {
        push @help_text, $_;
        next;
    }

    if (/^VERSION_MAJOR\s*=\s*(\S+)/) { $major = $1; }
    elsif (/^VERSION_MINOR\s*=\s*(\S+)/) { $minor = $1; }
    elsif (/^VERSION_PATCH\s*=\s*(\S+)/) { $patch = $1; }
    elsif (/^VERSION\s*=\s*(\S+)/) { $version = $1; }
}
close $cfg;

my $help_text = join('', @help_text);

# mkdir '' unless -d '';

open my $in, '<', 'version.hpp.in' or die $!;
open my $out, '>', 'version.hpp' or die $!;

while (<$in>) {
    s/\$\{VERSION_MAJOR\}/$major/g;
    s/\$\{VERSION_MINOR\}/$minor/g;
    s/\$\{VERSION_PATCH\}/$patch/g;
    s/\$\{VERSION\}/$version/g;
    print $out $_;
}

close $in;
close $out;
print "Generated: version.hpp\n";

open $in, '<', 'help_text.hpp.in' or die $!;
open $out, '>', 'help_text.hpp' or die $!;

while (<$in>) {
    if (/\$\{HELP_TEXT\}/) {
        my $indent = '';
        if (/^(\s*)/) {
            $indent = $1;
        }

        open my $tmpl, '<', 'help_text.hpp.in' or die $!;
        while (<$tmpl>) {
            if (/\$\{HELP_TEXT\}/) {

                my $line = $_;
                $line =~ s/\$\{HELP_TEXT\}/$help_text/;
                print $out $line;
            } else {
                print $out $_;
            }
        }

        close $tmpl;
        last;
    } else {
        print $out $_;
    }
}

close $in;
close $out;
print "Generated: help_text.hpp\n";