# 基于有限状态机模型的HTTP协议解析器

以下是在本项目的Parser中定义的6个状态，以及每个状态中处理的任务(Tasks)和状态转移路径(Transition)
![alt text](../assets/parserFSM.svg)
## 1. REQUEST_LINE
**Tasks:**
- Parse method, path, and version.
- Wait for CRLF.

**Transitions:**
| Event | Target State | Action |
| :--- | :--- | :--- |
| `CRLF` | `HEADERS` | `onRequestLine(method, path, version)` |

---

## 2. HEADERS
**Tasks:**
- Parse one header line.
- Check if it is a blank line.
- Decide whether there is a body or not.

**Transitions:**
| Event | Target State | Action |
| :--- | :--- | :--- |
| `Header-Line` | `HEADERS` | `onHeader(name, value)` |
| `CRLF` (Empty Line) | `COMPLETE` | `onHeadersComplete()`, `onMessageComplete()` |
| `CRLF` + `Content-Length` | `BODY_CONTENT_LENGTH` | `onHeadersComplete()` |
| `CRLF` + `chunked` | `BODY_CHUNKED` | `onHeadersComplete()` |

---

## 3. BODY_CONTENT_LENGTH
**Tasks:**
- Read N bytes (where N = Content-Length).

**Transitions:**
| Event | Target State | Action |
| :--- | :--- | :--- |
| `data` received | `BODY_CONTENT_LENGTH` | `onBody(data, len)` |
| `remaining == 0` | `COMPLETE` | `onMessageComplete()` |

---

## 4. BODY_CHUNKED
**Tasks:**
- *Not implemented*

---

## 5. COMPLETE
**Tasks:**
- End of processing.

---

## 6. ERROR
**Transitions:**
|Origin Event | Target State | Action |
| :--- | :--- | :--- |
|AnyState| error | onError(code)|