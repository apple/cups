/*
 * TLS support header for CUPS on macOS.
 *
 * Copyright © 2007-2019 by Apple Inc.
 * Copyright © 1997-2007 by Easy Software Products, all rights reserved.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/**** This file is included from tls-darwin.c ****/

extern char **environ;

#ifndef _SECURITY_VERSION_GREATER_THAN_57610_
typedef CF_OPTIONS(uint32_t, SecKeyUsage) {
    kSecKeyUsageAll              = 0x7FFFFFFF
};
#endif /* !_SECURITY_VERSION_GREATER_THAN_57610_ */
extern const void * kSecCSRChallengePassword;
extern const void * kSecSubjectAltName;
extern const void * kSecCertificateKeyUsage;
extern const void * kSecCSRBasicContraintsPathLen;
extern const void * kSecCertificateExtensions;
extern const void * kSecCertificateExtensionsEncoded;
extern const void * kSecOidCommonName;
extern const void * kSecOidCountryName;
extern const void * kSecOidStateProvinceName;
extern const void * kSecOidLocalityName;
extern const void * kSecOidOrganization;
extern const void * kSecOidOrganizationalUnit;
extern bool SecCertificateIsValid(SecCertificateRef certificate, CFAbsoluteTime verifyTime);
extern CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRef certificate);
extern SecCertificateRef SecGenerateSelfSignedCertificate(CFArrayRef subject, CFDictionaryRef parameters, SecKeyRef publicKey, SecKeyRef privateKey);
extern SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator, SecCertificateRef certificate, SecKeyRef privateKey);
