/*
 * Dynamic wrapper for Bonjour SDK for Windows.
 *
 * Copyright 2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

//#include <cups/http-private.h>
#include <cups/thread-private.h>
#include "dns_sd.h"


/*
 * Pointers for functions...
 */

static int		dnssd_initialized = 0;
static _cups_mutex_t	dnssd_mutex = _CUPS_MUTEX_INITIALIZER;
static DNSServiceErrorType (*dnssd_add_record)(DNSServiceRef sdRef, DNSRecordRef *RecordRef, DNSServiceFlags flags, uint16_t rrtype, uint16_t rdlen, const void *rdata, uint32_t ttl);
static DNSServiceErrorType (*dnssd_browse)(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *regtype, const char *domain, DNSServiceBrowseReply callBack, void *context);
static DNSServiceErrorType (*dnssd_construct_full_name)(char * const fullName, const char * const service, const char * const regtype, const char * const domain);
static DNSServiceErrorType (*dnssd_create_connection)(DNSServiceRef *sdRef);
static DNSServiceErrorType (*dnssd_process_result)(DNSServiceRef sdRef);
static DNSServiceErrorType (*dnssd_query_record)(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *fullname, uint16_t rrtype, uint16_t rrclass, DNSServiceQueryRecordReply callBack, void *context);
static void (*dnssd_deallocate)(DNSServiceRef sdRef);
static int (*dnssd_sock_fd)(DNSServiceRef sdRef);
static DNSServiceErrorType (*dnssd_register)(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *name, const char *regtype, const char *domain, const char *host, uint16_t port, uint16_t txtLen, const void *txtRecord, DNSServiceRegisterReply callBack, void *context);
static DNSServiceErrorType (*dnssd_remove_record)(DNSServiceRef sdRef, DNSRecordRef RecordRef, DNSServiceFlags flags);
static DNSServiceErrorType (*dnssd_resolve)(DNSServiceRef *sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, const char *name, const char *regtype, const char *domain, DNSServiceResolveReply callBack, void *context);
static DNSServiceErrorType (*dnssd_update_record)(DNSServiceRef sdRef, DNSRecordRef RecordRef, DNSServiceFlags flags, uint16_t rdlen, const void *rdata, uint32_t ttl);

static void (*dnssd_txt_create)(TXTRecordRef *txtRecord, uint16_t bufferLen, void *buffer);
static void (*dnssd_txt_deallocate)(TXTRecordRef *txtRecord);
static const void *(*dnssd_txt_get_bytes_ptr)(const TXTRecordRef *txtRecord);
static uint16_t (*dnssd_txt_get_count)(uint16_t txtLen, const void *txtRecord);
static uint16_t (*dnssd_txt_get_length)(const TXTRecordRef *txtRecord);
static DNSServiceErrorType (*dnssd_txt_get_item_at_index)(uint16_t txtLen, const void *txtRecord, uint16_t itemIndex, uint16_t keyBufLen, char *key, uint8_t *valueLen, const void **value);
static const void *(*dnssd_txt_get_value_ptr)(uint16_t txtLen, const void *txtRecord, const char *key, uint8_t *valueLen);
static DNSServiceErrorType (*dnssd_txt_set_value)(TXTRecordRef *txtRecord, const char *key, uint8_t valueSize, const void *value);


/*
 * Function to initialize pointers...
 */

static void
dnssd_init(void)
{
  _cupsMutexLock(&dnssd_mutex);
  if (!dnssd_initialized)
  {
    HINSTANCE	dll_handle = LoadLibraryA("dnssd.dll");

    if (dll_handle)
    {
      dnssd_add_record          = (DNSServiceErrorType (*)(DNSServiceRef, DNSRecordRef *, DNSServiceFlags, uint16_t, uint16_t, const void *, uint32_t))GetProcAddress(dll_handle, "DNSServiceAddRecord");
      dnssd_browse              = (DNSServiceErrorType(*)(DNSServiceRef *, DNSServiceFlags, uint32_t, const char *, const char *, DNSServiceBrowseReply, void *))GetProcAddress(dll_handle, "DNSServiceBrowse");
      dnssd_construct_full_name = (DNSServiceErrorType(*)(char * const, const char * const, const char * const, const char * const))GetProcAddress(dll_handle, "DNSServiceConstructFullName");
      dnssd_create_connection   = (DNSServiceErrorType(*)(DNSServiceRef *))GetProcAddress(dll_handle, "DNSServiceCreateConnection");
      dnssd_deallocate          = (DNSServiceErrorType(*)(DNSServiceRef))GetProcAddress(dll_handle, "DNSServiceRefDeallocate");
      dnssd_process_result      = (DNSServiceErrorType(*)(DNSServiceRef))GetProcAddress(dll_handle, "DNSServiceProcessResult");
      dnssd_query_record        = (DNSServiceErrorType(*)(DNSServiceRef *, DNSServiceFlags, uint32_t, const char *, uint16_t, uint16_t, DNSServiceQueryRecordReply, void *))GetProcAddress(dll_handle, "DNSServiceQueryRecord");
      dnssd_register            = (DNSServiceErrorType(*)(DNSServiceRef *, DNSServiceFlags, uint32_t, const char *, const char *, const char *, const char *, uint16_t, uint16_t, const void *, DNSServiceRegisterReply, void *))GetProcAddress(dll_handle, "DNSServiceRegister");
      dnssd_remove_record       = (DNSServiceErrorType(*)(DNSServiceRef, DNSRecordRef, DNSServiceFlags))GetProcAddress(dll_handle, "DNSServiceRemoveRecord");
      dnssd_resolve             = (DNSServiceErrorType(*)(DNSServiceRef *, DNSServiceFlags, uint32_t, const char *, const char *, const char *, DNSServiceResolveReply, void *))GetProcAddress(dll_handle, "DNSServiceResolve");
      dnssd_sock_fd             = (int(*)(DNSServiceRef))GetProcAddress(dll_handle, "DNSServiceRefSockFD");
      dnssd_update_record       = (DNSServiceErrorType(*)(DNSServiceRef, DNSRecordRef, DNSServiceFlags, uint16_t, const void *, uint32_t))GetProcAddress(dll_handle, "DNSServiceUpdateRecord");

      dnssd_txt_create          = (void (*)(TXTRecordRef *, uint16_t, void *))GetProcAddress(dll_handle, "TXTRecordCreate");
      dnssd_txt_deallocate      = (void (*)(TXTRecordRef *))GetProcAddress(dll_handle, "TXTRecordDeallocate");
      dnssd_txt_get_bytes_ptr   = (const void *(*)(const TXTRecordRef *))GetProcAddress(dll_handle, "TXTRecordGetBytesPtr");
      dnssd_txt_get_count       = (uint16_t (*)(uint16_t, const void *))GetProcAddress(dll_handle, "TXTRecordGetCount");
      dnssd_txt_get_item_at_index = (DNSServiceErrorType (*)(uint16_t, const void *, uint16_t, uint16_t, char *, uint8_t *, const void **))GetProcAddress(dll_handle, "TXTRecordGetItemAtIndex");
      dnssd_txt_get_length      = (uint16_t (*)(const TXTRecordRef *))GetProcAddress(dll_handle, "TXTRecordGetLength");
      dnssd_txt_get_value_ptr   = (const void *(*)(uint16_t, const void *, const char *, uint8_t *))GetProcAddress(dll_handle, "TXTRecordGetValuePtr");
      dnssd_txt_set_value       = (DNSServiceErrorType (*)(TXTRecordRef *, const char *, uint8_t, const void *))GetProcAddress(dll_handle, "TXTRecordSetValue");
    }

    dnssd_initialized = 1;
  }
  _cupsMutexUnlock(&dnssd_mutex);
}


// DNSServiceAddRecord
DNSServiceErrorType DNSSD_API DNSServiceAddRecord
    (
    DNSServiceRef                       sdRef,
    DNSRecordRef                        *RecordRef,
    DNSServiceFlags                     flags,
    uint16_t                            rrtype,
    uint16_t                            rdlen,
    const void                          *rdata,
    uint32_t                            ttl
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_add_record)
    return (*dnssd_add_record)(sdRef, RecordRef, flags, rrtype, rdlen, rdata, ttl);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceBrowse
DNSServiceErrorType DNSSD_API DNSServiceBrowse
    (
    DNSServiceRef                       *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *regtype,
    const char                          *domain,    /* may be NULL */
    DNSServiceBrowseReply               callBack,
    void                                *context    /* may be NULL */
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_browse)
    return (*dnssd_browse)(sdRef, flags, interfaceIndex, regtype, domain, callBack, context);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceConstructFullName
DNSServiceErrorType DNSSD_API DNSServiceConstructFullName
    (
    char                            * const fullName,
    const char                      * const service,      /* may be NULL */
    const char                      * const regtype,
    const char                      * const domain
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_construct_full_name)
    return (*dnssd_construct_full_name)(fullName, service, regtype, domain);
  else
    return (-1);
}


// DNSServiceCreateConnection
DNSServiceErrorType DNSSD_API DNSServiceCreateConnection(DNSServiceRef *sdRef)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_create_connection)
    return (*dnssd_create_connection)(sdRef);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceProcessResult
DNSServiceErrorType DNSSD_API DNSServiceProcessResult(DNSServiceRef sdRef)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_process_result)
    return (*dnssd_process_result)(sdRef);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceQueryRecord
DNSServiceErrorType DNSSD_API DNSServiceQueryRecord
    (
    DNSServiceRef                       *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *fullname,
    uint16_t                            rrtype,
    uint16_t                            rrclass,
    DNSServiceQueryRecordReply          callBack,
    void                                *context  /* may be NULL */
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_query_record)
    return (*dnssd_query_record)(sdRef, flags, interfaceIndex, fullname, rrtype, rrclass, callBack, context);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceRefDeallocate
void DNSSD_API DNSServiceRefDeallocate(DNSServiceRef sdRef)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_deallocate)
    (*dnssd_deallocate)(sdRef);
}


// DNSServiceRefSockFD
int DNSSD_API DNSServiceRefSockFD(DNSServiceRef sdRef)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_sock_fd)
    return (*dnssd_sock_fd)(sdRef);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceRegister
DNSServiceErrorType DNSSD_API DNSServiceRegister
    (
    DNSServiceRef                       *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *name,         /* may be NULL */
    const char                          *regtype,
    const char                          *domain,       /* may be NULL */
    const char                          *host,         /* may be NULL */
    uint16_t                            port,          /* In network byte order */
    uint16_t                            txtLen,
    const void                          *txtRecord,    /* may be NULL */
    DNSServiceRegisterReply             callBack,      /* may be NULL */
    void                                *context       /* may be NULL */
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_register)
    return (*dnssd_register)(sdRef, flags, interfaceIndex, name, regtype, domain, host, port, txtLen, txtRecord, callBack, context);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceRemoveRecord
DNSServiceErrorType DNSSD_API DNSServiceRemoveRecord
    (
    DNSServiceRef                 sdRef,
    DNSRecordRef                  RecordRef,
    DNSServiceFlags               flags
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_remove_record)
    return (*dnssd_remove_record)(sdRef, RecordRef, flags);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceResolve
DNSServiceErrorType DNSSD_API DNSServiceResolve
    (
    DNSServiceRef                       *sdRef,
    DNSServiceFlags                     flags,
    uint32_t                            interfaceIndex,
    const char                          *name,
    const char                          *regtype,
    const char                          *domain,
    DNSServiceResolveReply              callBack,
    void                                *context  /* may be NULL */
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_resolve)
    return (*dnssd_resolve)(sdRef, flags, interfaceIndex, name, regtype, domain, callBack, context);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// DNSServiceUpdateRecord
DNSServiceErrorType DNSSD_API DNSServiceUpdateRecord
    (
    DNSServiceRef                       sdRef,
    DNSRecordRef                        RecordRef,     /* may be NULL */
    DNSServiceFlags                     flags,
    uint16_t                            rdlen,
    const void                          *rdata,
    uint32_t                            ttl
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_update_record)
    return (*dnssd_update_record)(sdRef, RecordRef, flags, rdlen, rdata, ttl);
  else
    return (kDNSServiceErr_ServiceNotRunning);
}


// TXTRecordCreate
void DNSSD_API
TXTRecordCreate(
    TXTRecordRef     *txtRecord,
    uint16_t         bufferLen,
    void             *buffer)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_create)
    (*dnssd_txt_create)(txtRecord, bufferLen, buffer);
}


// TXTRecordDeallocate
void DNSSD_API TXTRecordDeallocate
    (
    TXTRecordRef     *txtRecord
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_deallocate)
    (*dnssd_txt_deallocate)(txtRecord);
}


// TXTRecordGetBytesPtr
const void * DNSSD_API TXTRecordGetBytesPtr
    (
    const TXTRecordRef *txtRecord
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_get_bytes_ptr)
    return (*dnssd_txt_get_bytes_ptr)(txtRecord);
  else
    return (NULL);
}


// TXTRecordGetLength
uint16_t DNSSD_API TXTRecordGetLength
    (
    const TXTRecordRef *txtRecord
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_get_length)
    return (*dnssd_txt_get_length)(txtRecord);
  else
    return (0);
}


// TXTRecordSetValue
DNSServiceErrorType DNSSD_API TXTRecordSetValue
    (
    TXTRecordRef     *txtRecord,
    const char       *key,
    uint8_t          valueSize,        /* may be zero */
    const void       *value            /* may be NULL */
    )
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_set_value)
    return (*dnssd_txt_set_value)(txtRecord, key, valueSize, value);
  else
    return (-1);
}


// TXTRecordGetCount
uint16_t DNSSD_API
TXTRecordGetCount(
    uint16_t         txtLen,
    const void       *txtRecord)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_get_count)
    return (*dnssd_txt_get_count)(txtLen, txtRecord);
  else
    return (0);
}


// TXTRecordGetItemAtIndex
DNSServiceErrorType DNSSD_API
TXTRecordGetItemAtIndex(
    uint16_t         txtLen,
    const void       *txtRecord,
    uint16_t         itemIndex,
    uint16_t         keyBufLen,
    char             *key,
    uint8_t          *valueLen,
    const void       **value)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_get_item_at_index)
    return (*dnssd_txt_get_item_at_index)(txtLen, txtRecord, itemIndex, keyBufLen, key, valueLen, value);
  else
    return (-1);
}


// TXTRecordGetValuePtr
const void * DNSSD_API
TXTRecordGetValuePtr(
    uint16_t         txtLen,
    const void       *txtRecord,
    const char       *key,
    uint8_t          *valueLen)
{
  if (!dnssd_initialized)
    dnssd_init();

  if (dnssd_txt_get_value_ptr)
    return (*dnssd_txt_get_value_ptr)(txtLen, txtRecord, key, valueLen);
  else
    return (NULL);
}
