package CUPS;

use 5.006;
use strict;
use warnings;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

our @ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use CUPS ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	CUPS_DATE_ANY
	CUPS_VERSION
	HTTP_MAX_BUFFER
	HTTP_MAX_HOST
	HTTP_MAX_URI
	HTTP_MAX_VALUE
	IPP_MAX_NAME
	IPP_MAX_VALUES
	IPP_PORT
	PPD_MAX_LINE
	PPD_MAX_NAME
	PPD_MAX_TEXT
	PPD_VERSION
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	CUPS_DATE_ANY
	CUPS_VERSION
	HTTP_MAX_BUFFER
	HTTP_MAX_HOST
	HTTP_MAX_URI
	HTTP_MAX_VALUE
	IPP_MAX_NAME
	IPP_MAX_VALUES
	IPP_PORT
	PPD_MAX_LINE
	PPD_MAX_NAME
	PPD_MAX_TEXT
	PPD_VERSION
);
our $VERSION = '1.2';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "& not defined" if $constname eq 'constant';
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/ || $!{EINVAL}) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    croak "Your vendor has not defined CUPS macro $constname";
	}
    }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
	if ($] >= 5.00561) {
	    *$AUTOLOAD = sub () { $val };
	}
	else {
	    *$AUTOLOAD = sub { $val };
	}
    }
    goto &$AUTOLOAD;
}

bootstrap CUPS $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

CUPS - Perl extension for blah blah blah

=head1 SYNOPSIS

  use CUPS;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for CUPS, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.

=head2 Exportable constants

  CUPS_DATE_ANY
  CUPS_VERSION
  HTTP_MAX_BUFFER
  HTTP_MAX_HOST
  HTTP_MAX_URI
  HTTP_MAX_VALUE
  IPP_MAX_NAME
  IPP_MAX_VALUES
  IPP_PORT
  PPD_MAX_LINE
  PPD_MAX_NAME
  PPD_MAX_TEXT
  PPD_VERSION


=head1 AUTHOR

A. U. Thor, E<lt>a.u.thor@a.galaxy.far.far.awayE<gt>

=head1 SEE ALSO

L<perl>.

=cut
