import core.demangle : demangle;
import core.runtime : rt_init;
import core.stdc.string : strlen;

/********************************
 * Wrapper for demangling D symbols with core.demangle.
 * Params:
 *  sym: C-string to mangled symbol
 *  buf: Buffer for demangled symbol
 *  buflen: Buffer size, it's set to the demangled length on return
 * Returns:
 *  Demangled string (no ending '\0') possibly equal to buf if buflen is long enough
 *  otherwise a GC allocated buffer that needs to be copied before re-entering
 *  this function (QByteArray does the job). Otherwise returns null.
 */
extern (C++) const(char)* ddemangle(const(char)* sym, char* buf, size_t* buflen)
{
    __gshared isInit = -1;
    if (isInit == -1)
        isInit = rt_init(); // to avoid multiple calls, rt_init() has a lock though
    if (isInit)
    {
        const name = demangle(sym[0 .. strlen(sym)], buf[0 .. *buflen]);
        *buflen = name.length;
        return name.ptr;
    }
    return null;
}
