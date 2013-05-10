/*
 * Simple test program that lists the Back to My Mac domains on a Mac.
 *
 * Compile with:
 *
 *   clang -o testbtmm -g testbtmm.c -framework SystemConfiguration -framework CoreFoundation
 */

#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>


/*
 * 'dnssdAddAlias()' - Add a DNS-SD alias name.
 */

static void
show_domain(const void *key,		/* I - Key */
	    const void *value,		/* I - Value (domain) */
	    void       *context)	/* I - Unused */
{
  char	valueStr[1024];			/* Domain string */


  (void)key;
  (void)context;

  if (CFGetTypeID((CFStringRef)value) == CFStringGetTypeID() &&
      CFStringGetCString((CFStringRef)value, valueStr, sizeof(valueStr),
                         kCFStringEncodingUTF8))
    printf("Back to My Mac domain: \"%s\"\n", valueStr);
  else
    puts("Bad Back to My Mac domain in dynamic store.");
}


int
main(void)
{
  SCDynamicStoreRef sc;			/* Context for dynamic store */
  CFDictionaryRef btmm;			/* Back-to-My-Mac domains */


  sc = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("cups"), NULL, NULL);

  if (!sc)
  {
    puts("Unable to open dynamic store.");
    exit(1);
  }

  btmm = SCDynamicStoreCopyValue(sc, CFSTR("Setup:/Network/BackToMyMac"));
  if (btmm && CFGetTypeID(btmm) == CFDictionaryGetTypeID())
  {
    printf("%d Back to My Mac domains.\n", (int)CFDictionaryGetCount(btmm));
    CFDictionaryApplyFunction(btmm, show_domain, NULL);
  }
  else if (btmm)
    puts("Bad Back to My Mac data in dynamic store.");
  else
    puts("No Back to My Mac domains.");

  return (1);
}
