#include <stdio.h>
#include <stdlib.h> // for rand(), exit()
#include <unistd.h> // for read(), write()
#include <fcntl.h>  /* for open() args */
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <stddef.h>
#include <stddef.h>
#include <dirent.h>
#include <libgen.h>

#ifdef X_OK
#undef X_OK
#endif /* X_OK */

#include "cdefs.h"
#include "kermit.h"
#include "unixio_rpi.h"
#include "fifo.h"
#include "debug.h"
#include "libcrc32_file.h"
#include <string.h> /* Must be after, for NULL */

static int ofile = -1;          /* File descriptors */
static FILE *ifile = (FILE *)0; /* and pointers */
#define KFILENAME_MAXSIZE 256
static char kfilename[KFILENAME_MAXSIZE] = {0};

/* DEBUG */
#ifdef DEBUG
static FILE *dp = (FILE *)0; /* Debug log */
static int xdebug = 0;       /* Debugging on/off */
static int tdebug = 0;

int dodebug(int fc, UCHAR *label, UCHAR *sval, long nval)
{
  if (fc != DB_OPN && fc != DB_OPNT && !xdebug)
    return (-1);
  if (!label)
    label = (UCHAR *)"";

  switch (fc)
  { /* Function code */
  case DB_OPNT:
    tdebug = 1;
  case DB_OPN: /* Open debug log */
    xdebug = 1;
    if (!*label)
      dp = stderr;
    else
    {
      if (dp)
        fclose(dp);
      dp = fopen((char *)label, "wt");
      if (!dp)
      {
        dp = stderr;
      }
      else
      {
        setbuf(dp, (char *)0); // cause dp to be unbuffered
      }
    }
    if (*label)
    {
      fprintf(dp, "DEBUG LOG OPEN: %s\n", label);
    }
    else
    {
      fprintf(dp, "DEBUG LOG OPEN: %s\n", "stderr");
    }
    return (0);
  case DB_MSG: /* Write a message */
    if (dp)
    {
      fprintf(dp, "%s\n", (char *)label);
    }
    return (0);
  case DB_CHR: /* Write label and character */
    if (dp)
    {
      if (nval > 32 && nval < 127)
      {
        fprintf(dp, "%s=[%c]\n", (char *)label, (char)nval);
      }
      else
      {
        fprintf(dp, "%s=[%d decimal]\n", (char *)label, (char)nval);
      }
    }
    return (0);
  case DB_PKT: /* Log a packet */
    if (!dp)
      return (0);
    if (sval)
    {
      int i;
      fprintf(dp, "%s[", (char *)label);
      for (i = 0; sval[i] != 0; i++)
      {
        int c = sval[i];
        if (c >= 32 && c < 127)
          fprintf(dp, "%c", c);
        else if (c == '\\')
          fprintf(dp, "\\\\");
        else if (c == 0x0d)
          fprintf(dp, "\\r");
        else if (c == 0x0a)
          fprintf(dp, "\\n");
        //          else if (c == '^')
        //             fprintf (dp, "\\^");
        //          else if (c >= 1 && c <= 26)
        //             fprintf (dp, "^%c",c + 64);
        else
          fprintf(dp, "[%02x]", c);
      }
      fprintf(dp, "]\n");
    }
    else
    {
      fprintf(dp, "%s=%ld\n", (char *)label, nval);
    }
    return (0);

  case DB_HEX: /* Log data in hex */
    if (!dp)
      return (0);
    if (sval)
    {
      int i;
      fprintf(dp, "%s[", (char *)label);
      for (i = 0; i < nval; i++)
      {
        int c = sval[i];
        if (i > 0)
          fprintf(dp, " ");
        fprintf(dp, "%02x", c);
      }
      fprintf(dp, "]\n");
      fprintf(dp, "  i=%d, nval=%ld\n", i, nval);
    }
    else
    {
      fprintf(dp, "%s=%ld\n", (char *)label, nval);
    }
    return (0);

  case DB_LOG: /* Write label and string or number */
    if (sval && dp)
      fprintf(dp, "%s[%s]\n", (char *)label, sval);
    else
      fprintf(dp, "%s=%ld\n", (char *)label, nval);
    return (0);
  case DB_CLS: /* Close debug log */
    if (dp)
    {
      fclose(dp);
      dp = (FILE *)0;
    }
    xdebug = 0;
  }
  return (-1);
}

#endif /* DEBUG */

static inline void kadd_rootpath(UCHAR *insert, UCHAR *src, char *dest, unsigned int max_size)
{
  /* insert must finish by a '/' */
  int len = 0;

  len = strlen((char *)insert);
  if (len)
  {
    if ((strlen((char *)src) + len) >= max_size)
    {
      dest[0] = 0;
      return;
    }
    strcpy((char *)dest, (char *)insert);
  }
  strcpy((char *)(dest + len), (char *)src);
  return;
}

/*-----------------------------------------------------------------------------
 * R E A D P K T -- Read a Kermit packet from the communications device
 *
 * Call with: k - Kermit struct pointer p - pointer to read buffer len -
 * length of read buffer
 *
 * When reading a packet, this function looks for start of Kermit packet
 * (k->r_soh), then reads everything between it and the end of the packet
 * (k->r_eom) into the indicated buffer.  Returns the number of bytes
 * read, or: 0 - timeout or other possibly correctable error; -1 - fatal
 * error, such as loss of connection, or no buffer to read into.
 *-----------------------------------------------------------------------------*/
int kreadpkt(struct k_data *k, UCHAR *p, int len)
{
  struct unixio_rpi *h = k->priv;
  uint8_t rx_fifo_char = 0, final_char = 0;
  UCHAR cmd[5] = {0};
  int32_t i = 0, comp = 1, n = 0, timeout = 0;
  short flag = 0;
  uint32_t err_code;

  debug(DB_LOG, "Entering kreadpkt", 0, 0);
  if (h->ble_mldp_get_byte == 0)
  {
    PRINT_DDEBUG("kreadpkt FAIL because ble_mldp_get_byte not init");
    return (X_ERROR);
  }
  if (h->ft_wait_ms == 0)
  {
    PRINT_DDEBUG("kreadpkt FAIL because ft_app_wait_ms not init");
    return (X_ERROR);
  }

  while (1)
  {
    timeout = k->r_timo;
    /* Set timeout in second with more than 5 retry */
    timeout = timeout * 1200;
    /* wait for the next character */

    do
    {
      err_code = h->ble_mldp_get_byte(&rx_fifo_char);
      if ((err_code != EXIT_FAILURE) && (err_code != EXIT_SUCCESS))
      {
        PRINT_DDEBUG("ble_mldp_get_byte error.");
        return (-K_END);
      }

      if (timeout >= 10)
      { // Better for low power
        timeout -= 10;
        h->ft_wait_ms(10);
      }
      else
      {
        timeout--;
        h->ft_wait_ms(1);
      }
    } while ((err_code == 1) && (timeout != 0));

    if (timeout == 0)
    {
      PRINT_DDEBUG("READPKT timeout fail, abort kermit...");
      return (0);
    }

    cmd[i] = rx_fifo_char;
    if (comp)
      i++;
    if ((i == 5) && (comp))
    {
      comp = 0;
      if (strncmp((char *)cmd, "CMD\r\n", 5) == 0)
      {
        PRINT_DDEBUG("CMD has been received, abort kermit...");
        return (-K_END);
      }
    }
    final_char = (k->parity) ? rx_fifo_char & 0x7f : rx_fifo_char & 0xff; /* Strip parity */

    if (!flag && final_char != k->r_soh) /* No start of packet yet */
                                         //if ((flag != 0) && (final_char != k->r_soh) && (final_char != k->r_eom))
      continue;                          /* so discard these bytes. */
    if (final_char == k->r_soh)
    { /* Start of packet */
      debug(DB_LOG, "START OF PACKET", 0, 0);
      flag = 1; /* Remember */
      continue; /* But discard. */
    }
    // Innes Kermit optimization
    else if (final_char == k->r_eom) /* Packet terminator */
    {
      debug(DB_LOG, "END OF PACKET", 0, 0);
      *p = NUL; /* Terminate for printing */
      debug(DB_LOG, "READPKT return", 0, n);
      return n;
    }
    else
    {                        /* Contents of packet */
      if (n++ > k->r_maxlen) /* Check length */
      {
        debug(DB_MSG, "READPKT return=0", 0, 0);
        PRINT_DDEBUG("READPKT return=0 ");
        // Innes Kermit optimization
        return n;
      }
      else
        *p++ = rx_fifo_char & 0xff;
    }
  }
  debug(DB_MSG, "READPKT FAIL (end)", 0, 0);
  PRINT_DDEBUG("READPKT FAIL (end)");
  return (-K_FAILURE);
}

/*-----------------------------------------------------------------------------
 * T X _ D A T A -- Writes n bytes of data to communication device.
 *
 * Call with: k = pointer to Kermit struct. p = pointer to data to
 * transmit. n = length. Returns: X_OK on success. X_ERROR on failure to
 * write - i/o error.
 *-----------------------------------------------------------------------------*/
int ktx_data(struct k_data *k, UCHAR *p, int n)
{
  int x = 0;
  struct unixio_rpi *h = k->priv;
  unsigned int timeout = k->s_timo * 1000; // Timeout in ms

  debug(DB_LOG, "TX_DATA write n=", 0, n);

  if (h->ble_mldp_send_bytes == 0)
  {
    PRINT_DDEBUG("ktx_data FAIL because ble_mldp_send_bytes not init");
    return (X_ERROR);
  }

  if (h->ble_mldp_send_bytes((const uint8_t *)p, (uint32_t)n) != 0)
  {
    PRINT_DDEBUG("ktx_data error\n");
    return X_ERROR;
  }
  return X_OK;
}

/*-----------------------------------------------------------------------------
 * O P E N F I L E -- Open output file
 *
 * Call with: Pointer to filename. Size in bytes. Creation date in format
 * yyyymmdd hh:mm:ss, e.g. 19950208 14:00:00 Mode: 1 = read, 2 = create, 3
 * = append. Returns: X_OK on success. X_ERROR on failure, including
 * rejection based on name, size, or date.
 *-----------------------------------------------------------------------------*/
int kopenfile(struct k_data *k, UCHAR *s, int mode, long filesize)
{
  PRINT_DDEBUG_ARG("Entering %s...", __FUNCTION__);
  debug(DB_LOG, "OPENFILE ", s, 0);
  debug(DB_LOG, "  mode", 0, mode);

  kadd_rootpath(k->rootpath, s, kfilename, KFILENAME_MAXSIZE);

  switch (mode)
  {
  case 1: /* Read */
    if (!(ifile = fopen((char *)kfilename, "r")))
    {
      debug(DB_LOG, "openfile read error", kfilename, 0);
      return (X_ERROR);
    }
    k->s_first = 1;        /* Set up for getkpt */
    k->zinbuf[0] = '\0';   /* Initialize buffer */
    k->zinptr = k->zinbuf; /* Set up buffer pointer */
    k->zincnt = 0;         /* and count */
    debug(DB_LOG, "openfile read ok", kfilename, 0);
    return (X_OK);

  case 2: /* Write (create) */
    PRINT_DDEBUG("kopenfile Write (create) not implemented");

  default:
    return (X_ERROR);
  }
}

/*-----------------------------------------------------------------------------
 * R E A D F I L E -- Read data from a file
 *-----------------------------------------------------------------------------*/
int kreadfile(struct k_data *k)
{
  //PRINT_DDEBUG_ARG("Entering %s...", __FUNCTION__);
  if (!k->zinptr)
  {
#ifdef DEBUG
    PRINT_DDEBUG("READFILE ZINPTR NOT SET");
    debug(DB_MSG, "READFILE ZINPTR NOT SET", 0, 0);
#endif /* DEBUG */
    return (-1);
  }
  if (k->zincnt < 1)
  { /* Nothing in buffer - must refill */
    if (k->binary)
    { /* Binary - just read raw buffers */
      k->dummy = 0;
      if (k->zinlen != 512)
      {
        PRINT_DDEBUG_ARG("READFILE should be 512, zinlen = %d", k->zinlen);
        debug(DB_LOG, "READFILE should be 512, zinlen", 0, k->zinlen);
        return (-1);
      }
      if (!(k->zincnt = fread(k->zinbuf, 1, k->zinlen, ifile)))
        return -1;
      /*for (int i = 0; i < k->zinlen; i++)
      {
        printf("%u", (unsigned int)k->zinbuf[i]);
      }*/
    }
    else
    {        /* Text mode needs LF/CRLF handling */
      int c; /* Current character */
      for (k->zincnt = 0; (k->zincnt < (k->zinlen - 2)); (k->zincnt)++)
      {
        if ((c = getc(ifile)) == EOF)
          break;
        if (c == '\n')                     /* Have newline? */
          k->zinbuf[(k->zincnt)++] = '\r'; /* Insert CR */
        k->zinbuf[k->zincnt] = c;
      }
#ifdef DEBUG
      k->zinbuf[k->zincnt] = '\0';
      PRINT_DDEBUG("READFILE text ok zincnt");
      debug(DB_LOG, "READFILE text ok zincnt", 0, k->zincnt);
#endif /* DEBUG */
    }
    k->zinbuf[k->zincnt] = '\0'; /* Terminate. */
    if (k->zincnt == 0)          /* Check for EOF */
      return (-1);
    k->zinptr = k->zinbuf; /* Not EOF - reset pointer */
  }
  (k->zincnt)--; /* Return first byte. */
  //PRINT_DDEBUG_ARG("READFILE exit zincnt = %d", k->zincnt);
  //PRINT_DDEBUG_ARG("READFILE exit zinptr = %d", k->zinptr);
  debug(DB_LOG, "READFILE exit zincnt", 0, k->zincnt);
  debug(DB_LOG, "READFILE exit zinptr", 0, k->zinptr);
  return (*(k->zinptr)++ & 0xff);
}

/*-----------------------------------------------------------------------------
 * F I L E I N F O -- Get info about existing file
 *
 * Call with: Pointer to filename Pointer to buffer for date-time string
 * Length of date-time string buffer (must be at least 18 bytes) Pointer
 * to int file type: 0: Prevailing type is text. 1: Prevailing type is
 * binary. Transfer mode (0 = auto, 1 = manual): 0: Figure out whether
 * file is text or binary and return type. 1: (nonzero) Don't try to
 * figure out file type. Returns: X_ERROR on failure. 0L or greater on
 * success == file length. Date-time string set to yyyymmdd hh:mm:ss
 * modtime of file. If date can't be determined, first byte of buffer is
 * set to NUL. Type set to 0 (text) or 1 (binary) if mode == 0.
 *-----------------------------------------------------------------------------*/
ULONG kfileinfo(struct k_data *k, UCHAR *filename, UCHAR *buf, int buflen, short *type, short mode)
{
  return X_OK;
}

/*-----------------------------------------------------------------------------
 * W R I T E F I L E -- Write data to file
 *
 * Call with: Kermit struct String pointer Length Returns: X_OK on success
 * X_ERROR on failure, such as i/o error, space used up, etc
 *-----------------------------------------------------------------------------*/
int kwritefile(struct k_data *k, UCHAR *s, int n)
{
  int rc = 0;
  rc = X_OK;

  debug(DB_LOG, "WRITEFILE n", 0, n);
  debug(DB_LOG, "WRITEFILE k->binary", 0, k->binary);

  if (k->binary)
  { /* Binary mode, just write it */
    debug(DB_HEX, "WHEX", s, n);
    if (write(ofile, s, n) != n)
      rc = X_ERROR;
  }
  else
  { /* Text mode, skip CRs */
    UCHAR *p, *q;
    int i;
    q = s;

    while (1)
    {
      for (p = q, i = 0; ((*p) && (*p != (UCHAR)13)); p++, i++)
        ;
      if (i > 0)
        if (write(ofile, q, i) != i)
          rc = X_ERROR;
      if (!*p)
        break;
      q = p + 1;
    }
  }
  return (rc);
}

/*-----------------------------------------------------------------------------
 * C L O S E F I L E -- Close output file
 *
 * For output files, the character c is the character (if any) from the Z
 * packet data field.  If it is D, it means the file transfer was canceled
 * in midstream by the sender, and the file is therefore incomplete.  This
 * routine should check for that and decide what to do.  It should be
 * harmless to call this routine for a file that that is not open.
 *-----------------------------------------------------------------------------*/
int kclosefile(struct k_data *k, UCHAR c, int mode)
{
  int rc = X_OK; /* Return code */

  debug(DB_LOG, "closefile mode", 0, mode);
  debug(DB_CHR, "closefile c", 0, c);

  switch (mode)
  {
  case 1:       /* Closing input file */
    if (!ifile) /* If not opened */
      break;
    debug(DB_LOG, "closefile (input)", k->filename, 0);
    if (fclose(ifile) < 0)
      rc = X_ERROR;
    ifile = (FILE *)0;
    break;
  case 2:          /* Closing output file */
    if (ofile < 0) /* If not opened */
      break;
    debug(DB_LOG, "closefile (output) name", k->filename, 0);
    debug(DB_LOG, "closefile (output) keep", 0, k->ikeep);
    if (close(ofile) < 0) /* Try to close */
      rc = X_ERROR;
    if ((k->ikeep == 0) && (c == 'D')) /* Don't keep incomplete files */
    {
      kadd_rootpath(k->rootpath, k->filename, kfilename, KFILENAME_MAXSIZE);
      if (k->filename)
        debug(DB_LOG, "deleting incomplete", kfilename, 0);
      unlink(kfilename); /* Delete it. */
    }
    break;
  default:
    rc = X_ERROR;
  }
  return (rc);
}

int kdevinit(void)
{
  PRINT_DDEBUG_ARG("Entering %s...", __FUNCTION__);
  return 0;
}

int kdevdeinit(void)
{
  PRINT_DDEBUG_ARG("Entering %s...", __FUNCTION__);
  return 0;
}

int kdevclose(void)
{
  return X_OK;
}

int kgetdirdata(struct k_data *k, char *pdf)
{

  DIR *dir = {0};
  int cnt = 0;
  struct dirent *ep = {0};
  unsigned long etag = 0;
  FILE *fp = NULL;

  kadd_rootpath(k->rootpath, k->dirname, kfilename, KFILENAME_MAXSIZE);
  dir = (DIR *)opendir(kfilename);
  PRINT_DDEBUG_ARG("Listing directory: %s", kfilename);

  if (dir == 0) /* Directory not accessible => error */
    return K_ERROR;

  strcpy(pdf, "[");

  while ((ep = readdir(dir)))
  { /* We show only the files on the first level of the asked directory */
    if ((ep->d_type != DT_REG) && (ep->d_type != DT_LNK))
      continue;
    /* Let's use k->xdatabuf as a temporary buffer */
    strcpy((char *)k->xdatabuf, kfilename);
    strcat((char *)k->xdatabuf, "/");
    strcat((char *)k->xdatabuf, ep->d_name);
    fp = fopen((char *)k->xdatabuf, "r");
    if (fp == NULL)
      continue;
    if (CRCF_calc_crc((CRCF_FILE *)fp, 0, 0, (uint32_t *)&etag))
    { // We accept 0-length files for the ones which begin with a dot (because they won't be uploaded). The others are rejected.
      if (ep->d_name[0] == 46)
        etag = 0;
      else
      {
        fclose(fp);
        continue;
      }
    }
    fclose(fp);
    if (cnt != 0)
      strcat(pdf, ",");
    strcat(pdf, "{\"filename\":\"");
    strcat(pdf, ep->d_name);
    strcat(pdf, "\",\"etag\":\"");
    sprintf((char *)k->xdatabuf, "%08lx", etag);
    strcat(pdf, (char *)k->xdatabuf);
    strcat(pdf, "\"}");
    cnt++;
    if (cnt == K_DIR_MAX)
      break;
  }
  strcat(pdf, "]\0");
  closedir(dir); /* Don't check return code */
  PRINT_DDEBUG_ARG("The result of DIR is : %s\n", pdf);
  return X_OK;
}

int kaccessfile(struct k_data *k, UCHAR *s)
{
  struct stat buf = {0};
  kadd_rootpath(k->rootpath, s, kfilename, KFILENAME_MAXSIZE);

  if (stat((const char *)kfilename, &buf) != 0)
  {
    PRINT_DDEBUG_ARG("The file '%s' is not ready to be read...\n", kfilename);
    perror("status");
    return X_ERROR;
  }
  PRINT_DDEBUG_ARG("The file '%s' is ready to be read\n", kfilename);
  return X_OK;
}

int kdevopen(void)
{
  return X_OK;
}