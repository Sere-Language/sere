; ModuleID = '_scratch\j_wsgi_mod.sere'
source_filename = "_scratch\\j_wsgi_mod.sere"
target triple = "x86_64-pc-windows-msvc"

%MultiMap = type { i32, ptr, ptr }
%Environ = type { i32, { ptr, i64 }, { ptr, i64 }, { ptr, i64 }, { ptr, i64 }, { ptr, i64 }, %MultiMap, %MultiMap }
%Response = type { i32, i32, { ptr, i64 }, { ptr, i64 }, { ptr, i64 }, %MultiMap }
%Route = type { i32, { ptr, i64 }, { ptr, i64 }, i32, ptr }
%Router = type { i32, ptr }
%Files = type { i32, { ptr, i64 }, { ptr, i64 } }
%Exception = type { i32, { ptr, i64 } }
%Server = type { i32, ptr }
%Counter = type { i32, i32 }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [2 x i8] c"&\00", align 1
@1 = private unnamed_addr constant [2 x i8] c"=\00", align 1
@2 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@3 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@4 = private unnamed_addr constant [2 x i8] c":\00", align 1
@5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@6 = private unnamed_addr constant [3 x i8] c": \00", align 1
@7 = private unnamed_addr constant [3 x i8] c"\0D\0A\00", align 1
@8 = private unnamed_addr constant [9 x i8] c"Continue\00", align 1
@9 = private unnamed_addr constant [20 x i8] c"Switching Protocols\00", align 1
@10 = private unnamed_addr constant [3 x i8] c"OK\00", align 1
@11 = private unnamed_addr constant [8 x i8] c"Created\00", align 1
@12 = private unnamed_addr constant [9 x i8] c"Accepted\00", align 1
@13 = private unnamed_addr constant [11 x i8] c"No Content\00", align 1
@14 = private unnamed_addr constant [14 x i8] c"Reset Content\00", align 1
@15 = private unnamed_addr constant [16 x i8] c"Partial Content\00", align 1
@16 = private unnamed_addr constant [18 x i8] c"Moved Permanently\00", align 1
@17 = private unnamed_addr constant [6 x i8] c"Found\00", align 1
@18 = private unnamed_addr constant [10 x i8] c"See Other\00", align 1
@19 = private unnamed_addr constant [13 x i8] c"Not Modified\00", align 1
@20 = private unnamed_addr constant [19 x i8] c"Temporary Redirect\00", align 1
@21 = private unnamed_addr constant [19 x i8] c"Permanent Redirect\00", align 1
@22 = private unnamed_addr constant [12 x i8] c"Bad Request\00", align 1
@23 = private unnamed_addr constant [13 x i8] c"Unauthorized\00", align 1
@24 = private unnamed_addr constant [10 x i8] c"Forbidden\00", align 1
@25 = private unnamed_addr constant [10 x i8] c"Not Found\00", align 1
@26 = private unnamed_addr constant [19 x i8] c"Method Not Allowed\00", align 1
@27 = private unnamed_addr constant [15 x i8] c"Not Acceptable\00", align 1
@28 = private unnamed_addr constant [16 x i8] c"Request Timeout\00", align 1
@29 = private unnamed_addr constant [9 x i8] c"Conflict\00", align 1
@30 = private unnamed_addr constant [5 x i8] c"Gone\00", align 1
@31 = private unnamed_addr constant [18 x i8] c"Payload Too Large\00", align 1
@32 = private unnamed_addr constant [23 x i8] c"Unsupported Media Type\00", align 1
@33 = private unnamed_addr constant [13 x i8] c"I'm a teapot\00", align 1
@34 = private unnamed_addr constant [21 x i8] c"Unprocessable Entity\00", align 1
@35 = private unnamed_addr constant [18 x i8] c"Too Many Requests\00", align 1
@36 = private unnamed_addr constant [22 x i8] c"Internal Server Error\00", align 1
@37 = private unnamed_addr constant [16 x i8] c"Not Implemented\00", align 1
@38 = private unnamed_addr constant [12 x i8] c"Bad Gateway\00", align 1
@39 = private unnamed_addr constant [20 x i8] c"Service Unavailable\00", align 1
@40 = private unnamed_addr constant [16 x i8] c"Gateway Timeout\00", align 1
@41 = private unnamed_addr constant [27 x i8] c"HTTP Version Not Supported\00", align 1
@42 = private unnamed_addr constant [3 x i8] c"OK\00", align 1
@43 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@44 = private unnamed_addr constant [2 x i8] c"\22\00", align 1
@45 = private unnamed_addr constant [3 x i8] c"\\\22\00", align 1
@46 = private unnamed_addr constant [2 x i8] c"\\\00", align 1
@47 = private unnamed_addr constant [3 x i8] c"\\\\\00", align 1
@48 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@49 = private unnamed_addr constant [3 x i8] c"\\n\00", align 1
@50 = private unnamed_addr constant [2 x i8] c"\0D\00", align 1
@51 = private unnamed_addr constant [3 x i8] c"\\r\00", align 1
@52 = private unnamed_addr constant [2 x i8] c"\09\00", align 1
@53 = private unnamed_addr constant [3 x i8] c"\\t\00", align 1
@54 = private unnamed_addr constant [2 x i8] c"\22\00", align 1
@55 = private unnamed_addr constant [2 x i8] c"\22\00", align 1
@56 = private unnamed_addr constant [6 x i8] c".html\00", align 1
@57 = private unnamed_addr constant [5 x i8] c".htm\00", align 1
@58 = private unnamed_addr constant [25 x i8] c"text/html; charset=utf-8\00", align 1
@59 = private unnamed_addr constant [5 x i8] c".css\00", align 1
@60 = private unnamed_addr constant [24 x i8] c"text/css; charset=utf-8\00", align 1
@61 = private unnamed_addr constant [4 x i8] c".js\00", align 1
@62 = private unnamed_addr constant [38 x i8] c"application/javascript; charset=utf-8\00", align 1
@63 = private unnamed_addr constant [6 x i8] c".json\00", align 1
@64 = private unnamed_addr constant [32 x i8] c"application/json; charset=utf-8\00", align 1
@65 = private unnamed_addr constant [5 x i8] c".txt\00", align 1
@66 = private unnamed_addr constant [26 x i8] c"text/plain; charset=utf-8\00", align 1
@67 = private unnamed_addr constant [5 x i8] c".svg\00", align 1
@68 = private unnamed_addr constant [14 x i8] c"image/svg+xml\00", align 1
@69 = private unnamed_addr constant [5 x i8] c".xml\00", align 1
@70 = private unnamed_addr constant [31 x i8] c"application/xml; charset=utf-8\00", align 1
@71 = private unnamed_addr constant [5 x i8] c".csv\00", align 1
@72 = private unnamed_addr constant [24 x i8] c"text/csv; charset=utf-8\00", align 1
@73 = private unnamed_addr constant [25 x i8] c"application/octet-stream\00", align 1
@74 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@75 = private unnamed_addr constant [9 x i8] c"HTTP/1.1\00", align 1
@76 = private unnamed_addr constant [4 x i8] c"get\00", align 1
@77 = private unnamed_addr constant [5 x i8] c"post\00", align 1
@78 = private unnamed_addr constant [5 x i8] c"head\00", align 1
@79 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@80 = private unnamed_addr constant [11 x i8] c"Set-Cookie\00", align 1
@81 = private unnamed_addr constant [2 x i8] c"=\00", align 1
@82 = private unnamed_addr constant [8 x i8] c"; Path=\00", align 1
@83 = private unnamed_addr constant [2 x i8] c" \00", align 1
@84 = private unnamed_addr constant [2 x i8] c" \00", align 1
@85 = private unnamed_addr constant [26 x i8] c"text/plain; charset=utf-8\00", align 1
@86 = private unnamed_addr constant [25 x i8] c"text/html; charset=utf-8\00", align 1
@87 = private unnamed_addr constant [32 x i8] c"application/json; charset=utf-8\00", align 1
@88 = private unnamed_addr constant [26 x i8] c"text/plain; charset=utf-8\00", align 1
@89 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@90 = private unnamed_addr constant [9 x i8] c"Location\00", align 1
@91 = private unnamed_addr constant [26 x i8] c"text/plain; charset=utf-8\00", align 1
@92 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@93 = private unnamed_addr constant [2 x i8] c"*\00", align 1
@94 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@95 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@96 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@97 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@98 = private unnamed_addr constant [2 x i8] c"{\00", align 1
@99 = private unnamed_addr constant [2 x i8] c":\00", align 1
@100 = private unnamed_addr constant [2 x i8] c"}\00", align 1
@101 = private unnamed_addr constant [2 x i8] c"*\00", align 1
@102 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@103 = private unnamed_addr constant [5 x i8] c"POST\00", align 1
@104 = private unnamed_addr constant [4 x i8] c"PUT\00", align 1
@105 = private unnamed_addr constant [6 x i8] c"PATCH\00", align 1
@106 = private unnamed_addr constant [7 x i8] c"DELETE\00", align 1
@107 = private unnamed_addr constant [2 x i8] c"*\00", align 1
@108 = private unnamed_addr constant [10 x i8] c"Not Found\00", align 1
@109 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@110 = private unnamed_addr constant [11 x i8] c"index.html\00", align 1
@111 = private unnamed_addr constant [3 x i8] c"..\00", align 1
@112 = private unnamed_addr constant [10 x i8] c"Forbidden\00", align 1
@113 = private unnamed_addr constant [10 x i8] c"Not Found\00", align 1
@114 = private unnamed_addr constant [10 x i8] c"Exception\00", align 1
@115 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@116 = private unnamed_addr constant [10 x i8] c"Exception\00", align 1
@117 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@118 = private unnamed_addr constant [5 x i8] c"home\00", align 1
@119 = private unnamed_addr constant [7 x i8] c"hello \00", align 1
@120 = private unnamed_addr constant [5 x i8] c"name\00", align 1
@121 = private unnamed_addr constant [6 x i8] c"world\00", align 1
@122 = private unnamed_addr constant [6 x i8] c"hits=\00", align 1
@123 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@124 = private unnamed_addr constant [7 x i8] c"/greet\00", align 1
@125 = private unnamed_addr constant [12 x i8] c"/users/{id}\00", align 1
@126 = private unnamed_addr constant [7 x i8] c"/count\00", align 1
@127 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@128 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@129 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@130 = private unnamed_addr constant [2 x i8] c" \00", align 1
@131 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@132 = private unnamed_addr constant [7 x i8] c"/greet\00", align 1
@133 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@134 = private unnamed_addr constant [10 x i8] c"name=sere\00", align 1
@135 = private unnamed_addr constant [2 x i8] c" \00", align 1
@136 = private unnamed_addr constant [5 x i8] c"POST\00", align 1
@137 = private unnamed_addr constant [7 x i8] c"/count\00", align 1
@138 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@139 = private unnamed_addr constant [2 x i8] c" \00", align 1
@140 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@141 = private unnamed_addr constant [6 x i8] c"/nope\00", align 1
@142 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@143 = private unnamed_addr constant [2 x i8] c" \00", align 1
@144 = private unnamed_addr constant [33 x i8] c"Host: example.com\0AX-Test:  1  \0A\0A\00", align 1
@145 = private unnamed_addr constant [2 x i8] c" \00", align 1
@146 = private unnamed_addr constant [5 x i8] c"host\00", align 1
@147 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@148 = private unnamed_addr constant [7 x i8] c"x-test\00", align 1
@149 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@150 = private unnamed_addr constant [2 x i8] c" \00", align 1
@151 = private unnamed_addr constant [6 x i8] c"a\22b\\c\00", align 1
@152 = private unnamed_addr constant [6 x i8] c"/home\00", align 1
@153 = private unnamed_addr constant [8 x i8] c"session\00", align 1
@154 = private unnamed_addr constant [4 x i8] c"abc\00", align 1
@155 = private unnamed_addr constant [2 x i8] c"/\00", align 1
@156 = private unnamed_addr constant [2 x i8] c" \00", align 1
@157 = private unnamed_addr constant [9 x i8] c"location\00", align 1
@158 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@159 = private unnamed_addr constant [11 x i8] c"set-cookie\00", align 1
@160 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@161 = private unnamed_addr constant [5 x i8] c"POST\00", align 1
@162 = private unnamed_addr constant [3 x i8] c"/x\00", align 1
@163 = private unnamed_addr constant [10 x i8] c"a=1&b=two\00", align 1
@164 = private unnamed_addr constant [2 x i8] c" \00", align 1
@165 = private unnamed_addr constant [2 x i8] c"a\00", align 1
@166 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@167 = private unnamed_addr constant [2 x i8] c"b\00", align 1
@168 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@169 = private unnamed_addr constant [2 x i8] c" \00", align 1
@170 = private unnamed_addr constant [8 x i8] c"a+b%2Fc\00", align 1
@171 = private unnamed_addr constant [7 x i8] c"app.js\00", align 1
@172 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@173 = private unnamed_addr constant [10 x i8] c"/users/42\00", align 1
@174 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@175 = private unnamed_addr constant [2 x i8] c" \00", align 1
@176 = private unnamed_addr constant [3 x i8] c"id\00", align 1
@177 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@178 = private unnamed_addr constant [8 x i8] c"/static\00", align 1
@179 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@180 = private unnamed_addr constant [15 x i8] c"/static/app.js\00", align 1
@181 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@182 = private unnamed_addr constant [2 x i8] c" \00", align 1
@183 = private unnamed_addr constant [4 x i8] c"GET\00", align 1
@184 = private unnamed_addr constant [8 x i8] c"/static\00", align 1
@185 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@186 = private unnamed_addr constant [2 x i8] c" \00", align 1

declare ptr @sere_http_listen(ptr, i64, i32)

declare ptr @sere_http_accept(ptr)

declare void @sere_http_req_method(ptr, ptr, ptr)

declare void @sere_http_req_path(ptr, ptr, ptr)

declare void @sere_http_req_query(ptr, ptr, ptr)

declare void @sere_http_req_version(ptr, ptr, ptr)

declare void @sere_http_req_headers(ptr, ptr, ptr)

declare void @sere_http_req_body(ptr, ptr, ptr)

declare void @sere_http_reply_ext(ptr, i32, ptr, i64, ptr, i64, ptr, i64, ptr, i64)

declare void @sere_http_reply(ptr, i32, ptr, i64, ptr, i64)

declare void @sere_http_close(ptr)

declare void @sere_url_decode(ptr, i64, ptr, ptr)

define void @wsgi_MultiMap___init__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %MultiMap, ptr %0, i32 0, i32 1
  %2 = call ptr @sere_list_new(i64 16)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store ptr %2, ptr %1, align 8
  %5 = load ptr, ptr %self.slot, align 8
  %6 = getelementptr inbounds nuw %MultiMap, ptr %5, i32 0, i32 2
  %7 = call ptr @sere_list_new(i64 16)
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret void

err.ok2:                                          ; preds = %err.ok
  store ptr %7, ptr %6, align 8
  ret void
}

define void @wsgi_MultiMap_add(ptr %self, { ptr, i64 } %name, { ptr, i64 } %value) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %value2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %MultiMap, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %MultiMap, ptr %3, i32 0, i32 1
  %5 = load ptr, ptr %4, align 8
  %6 = load { ptr, i64 }, ptr %name1, align 8
  %tmp.slot = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %6, ptr %tmp.slot, align 8
  call void @sere_list_push(ptr %5, ptr %tmp.slot)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  %9 = load ptr, ptr %self.slot, align 8
  %10 = getelementptr inbounds nuw %MultiMap, ptr %9, i32 0, i32 2
  %11 = load ptr, ptr %10, align 8
  %12 = load ptr, ptr %self.slot, align 8
  %13 = getelementptr inbounds nuw %MultiMap, ptr %12, i32 0, i32 2
  %14 = load ptr, ptr %13, align 8
  %15 = load { ptr, i64 }, ptr %value2, align 8
  %tmp.slot3 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %15, ptr %tmp.slot3, align 8
  call void @sere_list_push(ptr %14, ptr %tmp.slot3)
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret void

err.ok5:                                          ; preds = %err.ok
  ret void
}

define i64 @wsgi_MultiMap_count(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %MultiMap, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = call i64 @sere_list_len(ptr %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i64 0

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i64 0

err.ok2:                                          ; preds = %err.ok
  ret i64 %3
}

define i1 @wsgi_MultiMap_is_empty(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %MultiMap, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = call i64 @sere_list_len(ptr %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %6 = icmp eq i64 %3, 0
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i1 false

err.ok2:                                          ; preds = %err.ok
  ret i1 %6
}

define i64 @wsgi_MultiMap_index_of(ptr %self, { ptr, i64 } %name) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %target = alloca { ptr, i64 }, align 8
  %0 = load { ptr, i64 }, ptr %name1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %1, i64 %2, ptr %ext.str.data, ptr %ext.str.len)
  %3 = load i64, ptr %ext.str.len, align 4
  %4 = load ptr, ptr %ext.str.data, align 8
  %5 = insertvalue { ptr, i64 } undef, ptr %4, 0
  %6 = insertvalue { ptr, i64 } %5, i64 %3, 1
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i64 0

err.ok:                                           ; preds = %entry
  store { ptr, i64 } %6, ptr %target, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok13, %err.ok
  %9 = load i64, ptr %i, align 4
  %10 = load ptr, ptr %self.slot, align 8
  %11 = getelementptr inbounds nuw %MultiMap, ptr %10, i32 0, i32 1
  %12 = load ptr, ptr %11, align 8
  %13 = call i64 @sere_list_len(ptr %12)
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err2, label %err.ok3

while.body:                                       ; preds = %err.ok3
  %16 = load ptr, ptr %self.slot, align 8
  %17 = getelementptr inbounds nuw %MultiMap, ptr %16, i32 0, i32 1
  %18 = load ptr, ptr %17, align 8
  %19 = load i64, ptr %i, align 4
  %20 = call ptr @sere_list_item(ptr %18, i64 %19)
  %21 = load { ptr, i64 }, ptr %20, align 8
  %22 = extractvalue { ptr, i64 } %21, 0
  %23 = extractvalue { ptr, i64 } %21, 1
  %ext.str.data4 = alloca ptr, align 8
  %ext.str.len5 = alloca i64, align 8
  call void @sere_string_lower(ptr %22, i64 %23, ptr %ext.str.data4, ptr %ext.str.len5)
  %24 = load i64, ptr %ext.str.len5, align 4
  %25 = load ptr, ptr %ext.str.data4, align 8
  %26 = insertvalue { ptr, i64 } undef, ptr %25, 0
  %27 = insertvalue { ptr, i64 } %26, i64 %24, 1
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err6, label %err.ok7

while.end:                                        ; preds = %err.ok3
  %30 = call i32 @sere_has_error()
  %31 = icmp ne i32 %30, 0
  br i1 %31, label %err14, label %err.ok15

err2:                                             ; preds = %while.cond
  ret i64 0

err.ok3:                                          ; preds = %while.cond
  %32 = icmp slt i64 %9, %13
  br i1 %32, label %while.body, label %while.end

if.end:                                           ; preds = %if.next
  %33 = call i32 @sere_has_error()
  %34 = icmp ne i32 %33, 0
  br i1 %34, label %err10, label %err.ok11

err6:                                             ; preds = %while.body
  ret i64 0

err.ok7:                                          ; preds = %while.body
  %35 = load { ptr, i64 }, ptr %target, align 8
  %36 = extractvalue { ptr, i64 } %27, 0
  %37 = extractvalue { ptr, i64 } %27, 1
  %38 = extractvalue { ptr, i64 } %35, 0
  %39 = extractvalue { ptr, i64 } %35, 1
  %40 = call i32 @sere_str_cmp(ptr %36, i64 %37, ptr %38, i64 %39)
  %41 = icmp eq i32 %40, 0
  br i1 %41, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok7
  %42 = load i64, ptr %i, align 4
  %43 = call i32 @sere_has_error()
  %44 = icmp ne i32 %43, 0
  br i1 %44, label %err8, label %err.ok9

if.next:                                          ; preds = %err.ok7
  br label %if.end

err8:                                             ; preds = %if.then
  ret i64 0

err.ok9:                                          ; preds = %if.then
  ret i64 %42

err10:                                            ; preds = %if.end
  ret i64 0

err.ok11:                                         ; preds = %if.end
  %45 = load i64, ptr %i, align 4
  %46 = add i64 %45, 1
  store i64 %46, ptr %i, align 4
  %47 = call i32 @sere_has_error()
  %48 = icmp ne i32 %47, 0
  br i1 %48, label %err12, label %err.ok13

err12:                                            ; preds = %err.ok11
  ret i64 0

err.ok13:                                         ; preds = %err.ok11
  br label %while.cond

err14:                                            ; preds = %while.end
  ret i64 0

err.ok15:                                         ; preds = %while.end
  ret i64 -1
}

define i1 @wsgi_MultiMap_has(ptr %self, { ptr, i64 } %name) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %name1, align 8
  %2 = call i64 @wsgi_MultiMap_index_of(ptr %0, { ptr, i64 } %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %5 = icmp sge i64 %2, 0
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret i1 false

err.ok3:                                          ; preds = %err.ok
  ret i1 %5
}

define { ptr, i64 } @wsgi_MultiMap_get(ptr %self, { ptr, i64 } %name, { ptr, i64 } %fallback) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %fallback2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %fallback, ptr %fallback2, align 8
  %found = alloca i64, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %name1, align 8
  %2 = call i64 @wsgi_MultiMap_index_of(ptr %0, { ptr, i64 } %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  store i64 %2, ptr %found, align 4
  %5 = load i64, ptr %found, align 4
  %6 = icmp slt i64 %5, 0
  br i1 %6, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %7 = load ptr, ptr %self.slot, align 8
  %8 = getelementptr inbounds nuw %MultiMap, ptr %7, i32 0, i32 2
  %9 = load ptr, ptr %8, align 8
  %10 = load i64, ptr %found, align 4
  %11 = call ptr @sere_list_item(ptr %9, i64 %10)
  %12 = load { ptr, i64 }, ptr %11, align 8
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err5, label %err.ok6

if.then:                                          ; preds = %err.ok
  %15 = load { ptr, i64 }, ptr %fallback2, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

if.next:                                          ; preds = %err.ok
  br label %if.end

err3:                                             ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %if.then
  ret { ptr, i64 } %15

err5:                                             ; preds = %if.end
  ret { ptr, i64 } zeroinitializer

err.ok6:                                          ; preds = %if.end
  ret { ptr, i64 } %12
}

define ptr @wsgi_MultiMap_get_all(ptr %self, { ptr, i64 } %name) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %found_all = alloca ptr, align 8
  %0 = call ptr @sere_list_new(i64 16)
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret ptr null

err.ok:                                           ; preds = %entry
  store ptr %0, ptr %found_all, align 8
  %target = alloca { ptr, i64 }, align 8
  %3 = load { ptr, i64 }, ptr %name1, align 8
  %4 = extractvalue { ptr, i64 } %3, 0
  %5 = extractvalue { ptr, i64 } %3, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %4, i64 %5, ptr %ext.str.data, ptr %ext.str.len)
  %6 = load i64, ptr %ext.str.len, align 4
  %7 = load ptr, ptr %ext.str.data, align 8
  %8 = insertvalue { ptr, i64 } undef, ptr %7, 0
  %9 = insertvalue { ptr, i64 } %8, i64 %6, 1
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret ptr null

err.ok3:                                          ; preds = %err.ok
  store { ptr, i64 } %9, ptr %target, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok17, %err.ok3
  %12 = load i64, ptr %i, align 4
  %13 = load ptr, ptr %self.slot, align 8
  %14 = getelementptr inbounds nuw %MultiMap, ptr %13, i32 0, i32 1
  %15 = load ptr, ptr %14, align 8
  %16 = call i64 @sere_list_len(ptr %15)
  %17 = call i32 @sere_has_error()
  %18 = icmp ne i32 %17, 0
  br i1 %18, label %err4, label %err.ok5

while.body:                                       ; preds = %err.ok5
  %19 = load ptr, ptr %self.slot, align 8
  %20 = getelementptr inbounds nuw %MultiMap, ptr %19, i32 0, i32 1
  %21 = load ptr, ptr %20, align 8
  %22 = load i64, ptr %i, align 4
  %23 = call ptr @sere_list_item(ptr %21, i64 %22)
  %24 = load { ptr, i64 }, ptr %23, align 8
  %25 = extractvalue { ptr, i64 } %24, 0
  %26 = extractvalue { ptr, i64 } %24, 1
  %ext.str.data6 = alloca ptr, align 8
  %ext.str.len7 = alloca i64, align 8
  call void @sere_string_lower(ptr %25, i64 %26, ptr %ext.str.data6, ptr %ext.str.len7)
  %27 = load i64, ptr %ext.str.len7, align 4
  %28 = load ptr, ptr %ext.str.data6, align 8
  %29 = insertvalue { ptr, i64 } undef, ptr %28, 0
  %30 = insertvalue { ptr, i64 } %29, i64 %27, 1
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err8, label %err.ok9

while.end:                                        ; preds = %err.ok5
  %33 = load ptr, ptr %found_all, align 8
  %34 = call i32 @sere_has_error()
  %35 = icmp ne i32 %34, 0
  br i1 %35, label %err18, label %err.ok19

err4:                                             ; preds = %while.cond
  ret ptr null

err.ok5:                                          ; preds = %while.cond
  %36 = icmp slt i64 %12, %16
  br i1 %36, label %while.body, label %while.end

if.end:                                           ; preds = %if.next, %err.ok13
  %37 = call i32 @sere_has_error()
  %38 = icmp ne i32 %37, 0
  br i1 %38, label %err14, label %err.ok15

err8:                                             ; preds = %while.body
  ret ptr null

err.ok9:                                          ; preds = %while.body
  %39 = load { ptr, i64 }, ptr %target, align 8
  %40 = extractvalue { ptr, i64 } %30, 0
  %41 = extractvalue { ptr, i64 } %30, 1
  %42 = extractvalue { ptr, i64 } %39, 0
  %43 = extractvalue { ptr, i64 } %39, 1
  %44 = call i32 @sere_str_cmp(ptr %40, i64 %41, ptr %42, i64 %43)
  %45 = icmp eq i32 %44, 0
  br i1 %45, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok9
  %46 = load ptr, ptr %found_all, align 8
  %47 = load ptr, ptr %found_all, align 8
  %48 = load ptr, ptr %self.slot, align 8
  %49 = getelementptr inbounds nuw %MultiMap, ptr %48, i32 0, i32 2
  %50 = load ptr, ptr %49, align 8
  %51 = load i64, ptr %i, align 4
  %52 = call ptr @sere_list_item(ptr %50, i64 %51)
  %53 = load { ptr, i64 }, ptr %52, align 8
  %tmp.slot = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %53, ptr %tmp.slot, align 8
  call void @sere_list_push(ptr %47, ptr %tmp.slot)
  %54 = call i32 @sere_has_error()
  %55 = icmp ne i32 %54, 0
  br i1 %55, label %err10, label %err.ok11

if.next:                                          ; preds = %err.ok9
  br label %if.end

err10:                                            ; preds = %if.then
  ret ptr null

err.ok11:                                         ; preds = %if.then
  %56 = call i32 @sere_has_error()
  %57 = icmp ne i32 %56, 0
  br i1 %57, label %err12, label %err.ok13

err12:                                            ; preds = %err.ok11
  ret ptr null

err.ok13:                                         ; preds = %err.ok11
  br label %if.end

err14:                                            ; preds = %if.end
  ret ptr null

err.ok15:                                         ; preds = %if.end
  %58 = load i64, ptr %i, align 4
  %59 = add i64 %58, 1
  store i64 %59, ptr %i, align 4
  %60 = call i32 @sere_has_error()
  %61 = icmp ne i32 %60, 0
  br i1 %61, label %err16, label %err.ok17

err16:                                            ; preds = %err.ok15
  ret ptr null

err.ok17:                                         ; preds = %err.ok15
  br label %while.cond

err18:                                            ; preds = %while.end
  ret ptr null

err.ok19:                                         ; preds = %while.end
  ret ptr %33
}

define { ptr, i64 } @wsgi_MultiMap_name_at(ptr %self, i64 %index) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %index1 = alloca i64, align 8
  store i64 %index, ptr %index1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %MultiMap, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = load i64, ptr %index1, align 4
  %4 = call ptr @sere_list_item(ptr %2, i64 %3)
  %5 = load { ptr, i64 }, ptr %4, align 8
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  ret { ptr, i64 } %5
}

define { ptr, i64 } @wsgi_MultiMap_value_at(ptr %self, i64 %index) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %index1 = alloca i64, align 8
  store i64 %index, ptr %index1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %MultiMap, ptr %0, i32 0, i32 2
  %2 = load ptr, ptr %1, align 8
  %3 = load i64, ptr %index1, align 4
  %4 = call ptr @sere_list_item(ptr %2, i64 %3)
  %5 = load { ptr, i64 }, ptr %4, align 8
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  ret { ptr, i64 } %5
}

define void @wsgi_MultiMap_set(ptr %self, { ptr, i64 } %name, { ptr, i64 } %value) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %value2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value2, align 8
  %found = alloca i64, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %name1, align 8
  %2 = call i64 @wsgi_MultiMap_index_of(ptr %0, { ptr, i64 } %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store i64 %2, ptr %found, align 4
  %5 = load i64, ptr %found, align 4
  %6 = icmp slt i64 %5, 0
  br i1 %6, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %7 = load ptr, ptr %self.slot, align 8
  %8 = getelementptr inbounds nuw %MultiMap, ptr %7, i32 0, i32 2
  %9 = load ptr, ptr %8, align 8
  %10 = load i64, ptr %found, align 4
  %11 = call ptr @sere_list_item(ptr %9, i64 %10)
  %12 = load { ptr, i64 }, ptr %value2, align 8
  store { ptr, i64 } %12, ptr %11, align 8
  ret void

if.then:                                          ; preds = %err.ok
  %13 = load ptr, ptr %self.slot, align 8
  %14 = load { ptr, i64 }, ptr %name1, align 8
  %15 = load { ptr, i64 }, ptr %value2, align 8
  call void @wsgi_MultiMap_add(ptr %13, { ptr, i64 } %14, { ptr, i64 } %15)
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

if.next:                                          ; preds = %err.ok
  br label %if.end

err3:                                             ; preds = %if.then
  ret void

err.ok4:                                          ; preds = %if.then
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret void

err.ok6:                                          ; preds = %err.ok4
  %20 = call i32 @sere_has_error()
  %21 = icmp ne i32 %20, 0
  br i1 %21, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret void

err.ok8:                                          ; preds = %err.ok6
  ret void
}

define void @wsgi_MultiMap_remove(ptr %self, { ptr, i64 } %name) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %kept_names = alloca ptr, align 8
  %0 = call ptr @sere_list_new(i64 16)
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store ptr %0, ptr %kept_names, align 8
  %kept_values = alloca ptr, align 8
  %3 = call ptr @sere_list_new(i64 16)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret void

err.ok3:                                          ; preds = %err.ok
  store ptr %3, ptr %kept_values, align 8
  %target = alloca { ptr, i64 }, align 8
  %6 = load { ptr, i64 }, ptr %name1, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %7, i64 %8, ptr %ext.str.data, ptr %ext.str.len)
  %9 = load i64, ptr %ext.str.len, align 4
  %10 = load ptr, ptr %ext.str.data, align 8
  %11 = insertvalue { ptr, i64 } undef, ptr %10, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %9, 1
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok3
  ret void

err.ok5:                                          ; preds = %err.ok3
  store { ptr, i64 } %12, ptr %target, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok24, %err.ok5
  %15 = load i64, ptr %i, align 4
  %16 = load ptr, ptr %self.slot, align 8
  %17 = getelementptr inbounds nuw %MultiMap, ptr %16, i32 0, i32 1
  %18 = load ptr, ptr %17, align 8
  %19 = call i64 @sere_list_len(ptr %18)
  %20 = call i32 @sere_has_error()
  %21 = icmp ne i32 %20, 0
  br i1 %21, label %err6, label %err.ok7

while.body:                                       ; preds = %err.ok7
  %22 = load ptr, ptr %self.slot, align 8
  %23 = getelementptr inbounds nuw %MultiMap, ptr %22, i32 0, i32 1
  %24 = load ptr, ptr %23, align 8
  %25 = load i64, ptr %i, align 4
  %26 = call ptr @sere_list_item(ptr %24, i64 %25)
  %27 = load { ptr, i64 }, ptr %26, align 8
  %28 = extractvalue { ptr, i64 } %27, 0
  %29 = extractvalue { ptr, i64 } %27, 1
  %ext.str.data8 = alloca ptr, align 8
  %ext.str.len9 = alloca i64, align 8
  call void @sere_string_lower(ptr %28, i64 %29, ptr %ext.str.data8, ptr %ext.str.len9)
  %30 = load i64, ptr %ext.str.len9, align 4
  %31 = load ptr, ptr %ext.str.data8, align 8
  %32 = insertvalue { ptr, i64 } undef, ptr %31, 0
  %33 = insertvalue { ptr, i64 } %32, i64 %30, 1
  %34 = call i32 @sere_has_error()
  %35 = icmp ne i32 %34, 0
  br i1 %35, label %err10, label %err.ok11

while.end:                                        ; preds = %err.ok7
  %36 = load ptr, ptr %self.slot, align 8
  %37 = getelementptr inbounds nuw %MultiMap, ptr %36, i32 0, i32 1
  %38 = load ptr, ptr %kept_names, align 8
  store ptr %38, ptr %37, align 8
  %39 = load ptr, ptr %self.slot, align 8
  %40 = getelementptr inbounds nuw %MultiMap, ptr %39, i32 0, i32 2
  %41 = load ptr, ptr %kept_values, align 8
  store ptr %41, ptr %40, align 8
  ret void

err6:                                             ; preds = %while.cond
  ret void

err.ok7:                                          ; preds = %while.cond
  %42 = icmp slt i64 %15, %19
  br i1 %42, label %while.body, label %while.end

if.end:                                           ; preds = %if.next, %err.ok20
  %43 = call i32 @sere_has_error()
  %44 = icmp ne i32 %43, 0
  br i1 %44, label %err21, label %err.ok22

err10:                                            ; preds = %while.body
  ret void

err.ok11:                                         ; preds = %while.body
  %45 = load { ptr, i64 }, ptr %target, align 8
  %46 = extractvalue { ptr, i64 } %33, 0
  %47 = extractvalue { ptr, i64 } %33, 1
  %48 = extractvalue { ptr, i64 } %45, 0
  %49 = extractvalue { ptr, i64 } %45, 1
  %50 = call i32 @sere_str_cmp(ptr %46, i64 %47, ptr %48, i64 %49)
  %51 = icmp ne i32 %50, 0
  br i1 %51, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok11
  %52 = load ptr, ptr %kept_names, align 8
  %53 = load ptr, ptr %kept_names, align 8
  %54 = load ptr, ptr %self.slot, align 8
  %55 = getelementptr inbounds nuw %MultiMap, ptr %54, i32 0, i32 1
  %56 = load ptr, ptr %55, align 8
  %57 = load i64, ptr %i, align 4
  %58 = call ptr @sere_list_item(ptr %56, i64 %57)
  %59 = load { ptr, i64 }, ptr %58, align 8
  %tmp.slot = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %59, ptr %tmp.slot, align 8
  call void @sere_list_push(ptr %53, ptr %tmp.slot)
  %60 = call i32 @sere_has_error()
  %61 = icmp ne i32 %60, 0
  br i1 %61, label %err12, label %err.ok13

if.next:                                          ; preds = %err.ok11
  br label %if.end

err12:                                            ; preds = %if.then
  ret void

err.ok13:                                         ; preds = %if.then
  %62 = call i32 @sere_has_error()
  %63 = icmp ne i32 %62, 0
  br i1 %63, label %err14, label %err.ok15

err14:                                            ; preds = %err.ok13
  ret void

err.ok15:                                         ; preds = %err.ok13
  %64 = load ptr, ptr %kept_values, align 8
  %65 = load ptr, ptr %kept_values, align 8
  %66 = load ptr, ptr %self.slot, align 8
  %67 = getelementptr inbounds nuw %MultiMap, ptr %66, i32 0, i32 2
  %68 = load ptr, ptr %67, align 8
  %69 = load i64, ptr %i, align 4
  %70 = call ptr @sere_list_item(ptr %68, i64 %69)
  %71 = load { ptr, i64 }, ptr %70, align 8
  %tmp.slot16 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %71, ptr %tmp.slot16, align 8
  call void @sere_list_push(ptr %65, ptr %tmp.slot16)
  %72 = call i32 @sere_has_error()
  %73 = icmp ne i32 %72, 0
  br i1 %73, label %err17, label %err.ok18

err17:                                            ; preds = %err.ok15
  ret void

err.ok18:                                         ; preds = %err.ok15
  %74 = call i32 @sere_has_error()
  %75 = icmp ne i32 %74, 0
  br i1 %75, label %err19, label %err.ok20

err19:                                            ; preds = %err.ok18
  ret void

err.ok20:                                         ; preds = %err.ok18
  br label %if.end

err21:                                            ; preds = %if.end
  ret void

err.ok22:                                         ; preds = %if.end
  %76 = load i64, ptr %i, align 4
  %77 = add i64 %76, 1
  store i64 %77, ptr %i, align 4
  %78 = call i32 @sere_has_error()
  %79 = icmp ne i32 %78, 0
  br i1 %79, label %err23, label %err.ok24

err23:                                            ; preds = %err.ok22
  ret void

err.ok24:                                         ; preds = %err.ok22
  br label %while.cond
}

define %MultiMap @wsgi_parse_query({ ptr, i64 } %raw) {
entry:
  %raw1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %raw, ptr %raw1, align 8
  %out = alloca %MultiMap, align 8
  %init.tmp = alloca %MultiMap, align 8
  store %MultiMap zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %MultiMap, ptr %init.tmp, i32 0, i32 0
  store i32 1379361884, ptr %0, align 4
  call void @wsgi_MultiMap___init__(ptr %init.tmp)
  %1 = load %MultiMap, ptr %init.tmp, align 8
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %MultiMap zeroinitializer

err.ok:                                           ; preds = %entry
  store %MultiMap %1, ptr %out, align 8
  %4 = load { ptr, i64 }, ptr %raw1, align 8
  %5 = extractvalue { ptr, i64 } %4, 1
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err2, label %err.ok3

if.end:                                           ; preds = %if.next
  %pairs = alloca ptr, align 8
  %8 = load { ptr, i64 }, ptr %raw1, align 8
  %9 = extractvalue { ptr, i64 } %8, 0
  %10 = extractvalue { ptr, i64 } %8, 1
  %11 = call ptr @sere_string_split(ptr %9, i64 %10, ptr @0, i64 1)
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err6, label %err.ok7

err2:                                             ; preds = %err.ok
  ret %MultiMap zeroinitializer

err.ok3:                                          ; preds = %err.ok
  %14 = icmp eq i64 %5, 0
  br i1 %14, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok3
  %15 = load %MultiMap, ptr %out, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err4, label %err.ok5

if.next:                                          ; preds = %err.ok3
  br label %if.end

err4:                                             ; preds = %if.then
  ret %MultiMap zeroinitializer

err.ok5:                                          ; preds = %if.then
  ret %MultiMap %15

err6:                                             ; preds = %if.end
  ret %MultiMap zeroinitializer

err.ok7:                                          ; preds = %if.end
  store ptr %11, ptr %pairs, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok49, %err.ok7
  %18 = load i64, ptr %i, align 4
  %19 = load ptr, ptr %pairs, align 8
  %20 = call i64 @sere_list_len(ptr %19)
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err8, label %err.ok9

while.body:                                       ; preds = %err.ok9
  %pair = alloca { ptr, i64 }, align 8
  %23 = load ptr, ptr %pairs, align 8
  %24 = load i64, ptr %i, align 4
  %25 = call ptr @sere_list_item(ptr %23, i64 %24)
  %26 = load { ptr, i64 }, ptr %25, align 8
  store { ptr, i64 } %26, ptr %pair, align 8
  %27 = call i32 @sere_has_error()
  %28 = icmp ne i32 %27, 0
  br i1 %28, label %err10, label %err.ok11

while.end:                                        ; preds = %err.ok9
  %29 = load %MultiMap, ptr %out, align 8
  %30 = call i32 @sere_has_error()
  %31 = icmp ne i32 %30, 0
  br i1 %31, label %err50, label %err.ok51

err8:                                             ; preds = %while.cond
  ret %MultiMap zeroinitializer

err.ok9:                                          ; preds = %while.cond
  %32 = icmp slt i64 %18, %20
  br i1 %32, label %while.body, label %while.end

err10:                                            ; preds = %while.body
  ret %MultiMap zeroinitializer

err.ok11:                                         ; preds = %while.body
  %33 = load { ptr, i64 }, ptr %pair, align 8
  %34 = extractvalue { ptr, i64 } %33, 1
  %35 = call i32 @sere_has_error()
  %36 = icmp ne i32 %35, 0
  br i1 %36, label %err13, label %err.ok14

if.end12:                                         ; preds = %if.next16, %err.ok45
  %37 = call i32 @sere_has_error()
  %38 = icmp ne i32 %37, 0
  br i1 %38, label %err46, label %err.ok47

err13:                                            ; preds = %err.ok11
  ret %MultiMap zeroinitializer

err.ok14:                                         ; preds = %err.ok11
  %39 = icmp sgt i64 %34, 0
  br i1 %39, label %if.then15, label %if.next16

if.then15:                                        ; preds = %err.ok14
  %sep = alloca i64, align 8
  %40 = load { ptr, i64 }, ptr %pair, align 8
  %41 = extractvalue { ptr, i64 } %40, 0
  %42 = extractvalue { ptr, i64 } %40, 1
  %43 = call i64 @sere_string_find(ptr %41, i64 %42, ptr @1, i64 1)
  %44 = call i32 @sere_has_error()
  %45 = icmp ne i32 %44, 0
  br i1 %45, label %err17, label %err.ok18

if.next16:                                        ; preds = %err.ok14
  br label %if.end12

err17:                                            ; preds = %if.then15
  ret %MultiMap zeroinitializer

err.ok18:                                         ; preds = %if.then15
  store i64 %43, ptr %sep, align 4
  %46 = call i32 @sere_has_error()
  %47 = icmp ne i32 %46, 0
  br i1 %47, label %err19, label %err.ok20

err19:                                            ; preds = %err.ok18
  ret %MultiMap zeroinitializer

err.ok20:                                         ; preds = %err.ok18
  %48 = load i64, ptr %sep, align 4
  %49 = icmp sge i64 %48, 0
  br i1 %49, label %if.then22, label %if.next23

if.end21:                                         ; preds = %err.ok43, %err.ok35
  %50 = call i32 @sere_has_error()
  %51 = icmp ne i32 %50, 0
  br i1 %51, label %err44, label %err.ok45

if.then22:                                        ; preds = %err.ok20
  %52 = load { ptr, i64 }, ptr %pair, align 8
  %sl.data = alloca ptr, align 8
  %sl.len = alloca i64, align 8
  %53 = load i64, ptr %sep, align 4
  %54 = extractvalue { ptr, i64 } %52, 0
  %55 = extractvalue { ptr, i64 } %52, 1
  call void @sere_str_slice(ptr %54, i64 %55, i64 0, i64 %53, i32 1, i32 1, ptr %sl.data, ptr %sl.len)
  %56 = load i64, ptr %sl.len, align 4
  %57 = load ptr, ptr %sl.data, align 8
  %58 = insertvalue { ptr, i64 } undef, ptr %57, 0
  %59 = insertvalue { ptr, i64 } %58, i64 %56, 1
  %60 = extractvalue { ptr, i64 } %59, 0
  %61 = extractvalue { ptr, i64 } %59, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_url_decode(ptr %60, i64 %61, ptr %ext.str.data, ptr %ext.str.len)
  %62 = load i64, ptr %ext.str.len, align 4
  %63 = load ptr, ptr %ext.str.data, align 8
  %64 = insertvalue { ptr, i64 } undef, ptr %63, 0
  %65 = insertvalue { ptr, i64 } %64, i64 %62, 1
  %66 = call i32 @sere_has_error()
  %67 = icmp ne i32 %66, 0
  br i1 %67, label %err24, label %err.ok25

if.next23:                                        ; preds = %err.ok20
  %68 = load { ptr, i64 }, ptr %pair, align 8
  %69 = extractvalue { ptr, i64 } %68, 0
  %70 = extractvalue { ptr, i64 } %68, 1
  %ext.str.data36 = alloca ptr, align 8
  %ext.str.len37 = alloca i64, align 8
  call void @sere_url_decode(ptr %69, i64 %70, ptr %ext.str.data36, ptr %ext.str.len37)
  %71 = load i64, ptr %ext.str.len37, align 4
  %72 = load ptr, ptr %ext.str.data36, align 8
  %73 = insertvalue { ptr, i64 } undef, ptr %72, 0
  %74 = insertvalue { ptr, i64 } %73, i64 %71, 1
  %75 = call i32 @sere_has_error()
  %76 = icmp ne i32 %75, 0
  br i1 %76, label %err38, label %err.ok39

err24:                                            ; preds = %if.then22
  ret %MultiMap zeroinitializer

err.ok25:                                         ; preds = %if.then22
  %77 = load { ptr, i64 }, ptr %pair, align 8
  %sl.data26 = alloca ptr, align 8
  %sl.len27 = alloca i64, align 8
  %78 = load i64, ptr %sep, align 4
  %79 = add i64 %78, 1
  %80 = extractvalue { ptr, i64 } %77, 0
  %81 = extractvalue { ptr, i64 } %77, 1
  call void @sere_str_slice(ptr %80, i64 %81, i64 %79, i64 0, i32 1, i32 0, ptr %sl.data26, ptr %sl.len27)
  %82 = load i64, ptr %sl.len27, align 4
  %83 = load ptr, ptr %sl.data26, align 8
  %84 = insertvalue { ptr, i64 } undef, ptr %83, 0
  %85 = insertvalue { ptr, i64 } %84, i64 %82, 1
  %86 = extractvalue { ptr, i64 } %85, 0
  %87 = extractvalue { ptr, i64 } %85, 1
  %ext.str.data28 = alloca ptr, align 8
  %ext.str.len29 = alloca i64, align 8
  call void @sere_url_decode(ptr %86, i64 %87, ptr %ext.str.data28, ptr %ext.str.len29)
  %88 = load i64, ptr %ext.str.len29, align 4
  %89 = load ptr, ptr %ext.str.data28, align 8
  %90 = insertvalue { ptr, i64 } undef, ptr %89, 0
  %91 = insertvalue { ptr, i64 } %90, i64 %88, 1
  %92 = call i32 @sere_has_error()
  %93 = icmp ne i32 %92, 0
  br i1 %93, label %err30, label %err.ok31

err30:                                            ; preds = %err.ok25
  ret %MultiMap zeroinitializer

err.ok31:                                         ; preds = %err.ok25
  call void @wsgi_MultiMap_add(ptr %out, { ptr, i64 } %65, { ptr, i64 } %91)
  %94 = call i32 @sere_has_error()
  %95 = icmp ne i32 %94, 0
  br i1 %95, label %err32, label %err.ok33

err32:                                            ; preds = %err.ok31
  ret %MultiMap zeroinitializer

err.ok33:                                         ; preds = %err.ok31
  %96 = call i32 @sere_has_error()
  %97 = icmp ne i32 %96, 0
  br i1 %97, label %err34, label %err.ok35

err34:                                            ; preds = %err.ok33
  ret %MultiMap zeroinitializer

err.ok35:                                         ; preds = %err.ok33
  br label %if.end21

err38:                                            ; preds = %if.next23
  ret %MultiMap zeroinitializer

err.ok39:                                         ; preds = %if.next23
  call void @wsgi_MultiMap_add(ptr %out, { ptr, i64 } %74, { ptr, i64 } { ptr @2, i64 0 })
  %98 = call i32 @sere_has_error()
  %99 = icmp ne i32 %98, 0
  br i1 %99, label %err40, label %err.ok41

err40:                                            ; preds = %err.ok39
  ret %MultiMap zeroinitializer

err.ok41:                                         ; preds = %err.ok39
  %100 = call i32 @sere_has_error()
  %101 = icmp ne i32 %100, 0
  br i1 %101, label %err42, label %err.ok43

err42:                                            ; preds = %err.ok41
  ret %MultiMap zeroinitializer

err.ok43:                                         ; preds = %err.ok41
  br label %if.end21

err44:                                            ; preds = %if.end21
  ret %MultiMap zeroinitializer

err.ok45:                                         ; preds = %if.end21
  br label %if.end12

err46:                                            ; preds = %if.end12
  ret %MultiMap zeroinitializer

err.ok47:                                         ; preds = %if.end12
  %102 = load i64, ptr %i, align 4
  %103 = add i64 %102, 1
  store i64 %103, ptr %i, align 4
  %104 = call i32 @sere_has_error()
  %105 = icmp ne i32 %104, 0
  br i1 %105, label %err48, label %err.ok49

err48:                                            ; preds = %err.ok47
  ret %MultiMap zeroinitializer

err.ok49:                                         ; preds = %err.ok47
  br label %while.cond

err50:                                            ; preds = %while.end
  ret %MultiMap zeroinitializer

err.ok51:                                         ; preds = %while.end
  ret %MultiMap %29
}

define %MultiMap @wsgi_parse_headers({ ptr, i64 } %raw) {
entry:
  %raw1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %raw, ptr %raw1, align 8
  %out = alloca %MultiMap, align 8
  %init.tmp = alloca %MultiMap, align 8
  store %MultiMap zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %MultiMap, ptr %init.tmp, i32 0, i32 0
  store i32 1379361884, ptr %0, align 4
  call void @wsgi_MultiMap___init__(ptr %init.tmp)
  %1 = load %MultiMap, ptr %init.tmp, align 8
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %MultiMap zeroinitializer

err.ok:                                           ; preds = %entry
  store %MultiMap %1, ptr %out, align 8
  %lines = alloca ptr, align 8
  %4 = load { ptr, i64 }, ptr %raw1, align 8
  %5 = extractvalue { ptr, i64 } %4, 0
  %6 = extractvalue { ptr, i64 } %4, 1
  %7 = call ptr @sere_string_split(ptr %5, i64 %6, ptr @3, i64 1)
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %MultiMap zeroinitializer

err.ok3:                                          ; preds = %err.ok
  store ptr %7, ptr %lines, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok49, %err.ok3
  %10 = load i64, ptr %i, align 4
  %11 = load ptr, ptr %lines, align 8
  %12 = call i64 @sere_list_len(ptr %11)
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err4, label %err.ok5

while.body:                                       ; preds = %err.ok5
  %line = alloca { ptr, i64 }, align 8
  %15 = load ptr, ptr %lines, align 8
  %16 = load i64, ptr %i, align 4
  %17 = call ptr @sere_list_item(ptr %15, i64 %16)
  %18 = load { ptr, i64 }, ptr %17, align 8
  %19 = extractvalue { ptr, i64 } %18, 0
  %20 = extractvalue { ptr, i64 } %18, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_strip(ptr %19, i64 %20, ptr %ext.str.data, ptr %ext.str.len)
  %21 = load i64, ptr %ext.str.len, align 4
  %22 = load ptr, ptr %ext.str.data, align 8
  %23 = insertvalue { ptr, i64 } undef, ptr %22, 0
  %24 = insertvalue { ptr, i64 } %23, i64 %21, 1
  %25 = call i32 @sere_has_error()
  %26 = icmp ne i32 %25, 0
  br i1 %26, label %err6, label %err.ok7

while.end:                                        ; preds = %err.ok5
  %27 = load %MultiMap, ptr %out, align 8
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err50, label %err.ok51

err4:                                             ; preds = %while.cond
  ret %MultiMap zeroinitializer

err.ok5:                                          ; preds = %while.cond
  %30 = icmp slt i64 %10, %12
  br i1 %30, label %while.body, label %while.end

err6:                                             ; preds = %while.body
  ret %MultiMap zeroinitializer

err.ok7:                                          ; preds = %while.body
  store { ptr, i64 } %24, ptr %line, align 8
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err8, label %err.ok9

err8:                                             ; preds = %err.ok7
  ret %MultiMap zeroinitializer

err.ok9:                                          ; preds = %err.ok7
  %33 = load { ptr, i64 }, ptr %line, align 8
  %34 = extractvalue { ptr, i64 } %33, 1
  %35 = call i32 @sere_has_error()
  %36 = icmp ne i32 %35, 0
  br i1 %36, label %err10, label %err.ok11

if.end:                                           ; preds = %if.next, %err.ok45
  %37 = call i32 @sere_has_error()
  %38 = icmp ne i32 %37, 0
  br i1 %38, label %err46, label %err.ok47

err10:                                            ; preds = %err.ok9
  ret %MultiMap zeroinitializer

err.ok11:                                         ; preds = %err.ok9
  %39 = icmp sgt i64 %34, 0
  br i1 %39, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok11
  %sep = alloca i64, align 8
  %40 = load { ptr, i64 }, ptr %line, align 8
  %41 = extractvalue { ptr, i64 } %40, 0
  %42 = extractvalue { ptr, i64 } %40, 1
  %43 = call i64 @sere_string_find(ptr %41, i64 %42, ptr @4, i64 1)
  %44 = call i32 @sere_has_error()
  %45 = icmp ne i32 %44, 0
  br i1 %45, label %err12, label %err.ok13

if.next:                                          ; preds = %err.ok11
  br label %if.end

err12:                                            ; preds = %if.then
  ret %MultiMap zeroinitializer

err.ok13:                                         ; preds = %if.then
  store i64 %43, ptr %sep, align 4
  %46 = call i32 @sere_has_error()
  %47 = icmp ne i32 %46, 0
  br i1 %47, label %err14, label %err.ok15

err14:                                            ; preds = %err.ok13
  ret %MultiMap zeroinitializer

err.ok15:                                         ; preds = %err.ok13
  %48 = load i64, ptr %sep, align 4
  %49 = icmp sge i64 %48, 0
  br i1 %49, label %if.then17, label %if.next18

if.end16:                                         ; preds = %if.next18, %err.ok43
  %50 = call i32 @sere_has_error()
  %51 = icmp ne i32 %50, 0
  br i1 %51, label %err44, label %err.ok45

if.then17:                                        ; preds = %err.ok15
  %name = alloca { ptr, i64 }, align 8
  %52 = load { ptr, i64 }, ptr %line, align 8
  %sl.data = alloca ptr, align 8
  %sl.len = alloca i64, align 8
  %53 = load i64, ptr %sep, align 4
  %54 = extractvalue { ptr, i64 } %52, 0
  %55 = extractvalue { ptr, i64 } %52, 1
  call void @sere_str_slice(ptr %54, i64 %55, i64 0, i64 %53, i32 1, i32 1, ptr %sl.data, ptr %sl.len)
  %56 = load i64, ptr %sl.len, align 4
  %57 = load ptr, ptr %sl.data, align 8
  %58 = insertvalue { ptr, i64 } undef, ptr %57, 0
  %59 = insertvalue { ptr, i64 } %58, i64 %56, 1
  %60 = extractvalue { ptr, i64 } %59, 0
  %61 = extractvalue { ptr, i64 } %59, 1
  %ext.str.data19 = alloca ptr, align 8
  %ext.str.len20 = alloca i64, align 8
  call void @sere_string_strip(ptr %60, i64 %61, ptr %ext.str.data19, ptr %ext.str.len20)
  %62 = load i64, ptr %ext.str.len20, align 4
  %63 = load ptr, ptr %ext.str.data19, align 8
  %64 = insertvalue { ptr, i64 } undef, ptr %63, 0
  %65 = insertvalue { ptr, i64 } %64, i64 %62, 1
  %66 = call i32 @sere_has_error()
  %67 = icmp ne i32 %66, 0
  br i1 %67, label %err21, label %err.ok22

if.next18:                                        ; preds = %err.ok15
  br label %if.end16

err21:                                            ; preds = %if.then17
  ret %MultiMap zeroinitializer

err.ok22:                                         ; preds = %if.then17
  store { ptr, i64 } %65, ptr %name, align 8
  %68 = call i32 @sere_has_error()
  %69 = icmp ne i32 %68, 0
  br i1 %69, label %err23, label %err.ok24

err23:                                            ; preds = %err.ok22
  ret %MultiMap zeroinitializer

err.ok24:                                         ; preds = %err.ok22
  %value = alloca { ptr, i64 }, align 8
  %70 = load { ptr, i64 }, ptr %line, align 8
  %sl.data25 = alloca ptr, align 8
  %sl.len26 = alloca i64, align 8
  %71 = load i64, ptr %sep, align 4
  %72 = add i64 %71, 1
  %73 = extractvalue { ptr, i64 } %70, 0
  %74 = extractvalue { ptr, i64 } %70, 1
  call void @sere_str_slice(ptr %73, i64 %74, i64 %72, i64 0, i32 1, i32 0, ptr %sl.data25, ptr %sl.len26)
  %75 = load i64, ptr %sl.len26, align 4
  %76 = load ptr, ptr %sl.data25, align 8
  %77 = insertvalue { ptr, i64 } undef, ptr %76, 0
  %78 = insertvalue { ptr, i64 } %77, i64 %75, 1
  %79 = extractvalue { ptr, i64 } %78, 0
  %80 = extractvalue { ptr, i64 } %78, 1
  %ext.str.data27 = alloca ptr, align 8
  %ext.str.len28 = alloca i64, align 8
  call void @sere_string_strip(ptr %79, i64 %80, ptr %ext.str.data27, ptr %ext.str.len28)
  %81 = load i64, ptr %ext.str.len28, align 4
  %82 = load ptr, ptr %ext.str.data27, align 8
  %83 = insertvalue { ptr, i64 } undef, ptr %82, 0
  %84 = insertvalue { ptr, i64 } %83, i64 %81, 1
  %85 = call i32 @sere_has_error()
  %86 = icmp ne i32 %85, 0
  br i1 %86, label %err29, label %err.ok30

err29:                                            ; preds = %err.ok24
  ret %MultiMap zeroinitializer

err.ok30:                                         ; preds = %err.ok24
  store { ptr, i64 } %84, ptr %value, align 8
  %87 = call i32 @sere_has_error()
  %88 = icmp ne i32 %87, 0
  br i1 %88, label %err31, label %err.ok32

err31:                                            ; preds = %err.ok30
  ret %MultiMap zeroinitializer

err.ok32:                                         ; preds = %err.ok30
  %89 = load { ptr, i64 }, ptr %name, align 8
  %90 = extractvalue { ptr, i64 } %89, 1
  %91 = call i32 @sere_has_error()
  %92 = icmp ne i32 %91, 0
  br i1 %92, label %err34, label %err.ok35

if.end33:                                         ; preds = %if.next37, %err.ok41
  %93 = call i32 @sere_has_error()
  %94 = icmp ne i32 %93, 0
  br i1 %94, label %err42, label %err.ok43

err34:                                            ; preds = %err.ok32
  ret %MultiMap zeroinitializer

err.ok35:                                         ; preds = %err.ok32
  %95 = icmp sgt i64 %90, 0
  br i1 %95, label %if.then36, label %if.next37

if.then36:                                        ; preds = %err.ok35
  %96 = load { ptr, i64 }, ptr %name, align 8
  %97 = load { ptr, i64 }, ptr %value, align 8
  call void @wsgi_MultiMap_add(ptr %out, { ptr, i64 } %96, { ptr, i64 } %97)
  %98 = call i32 @sere_has_error()
  %99 = icmp ne i32 %98, 0
  br i1 %99, label %err38, label %err.ok39

if.next37:                                        ; preds = %err.ok35
  br label %if.end33

err38:                                            ; preds = %if.then36
  ret %MultiMap zeroinitializer

err.ok39:                                         ; preds = %if.then36
  %100 = call i32 @sere_has_error()
  %101 = icmp ne i32 %100, 0
  br i1 %101, label %err40, label %err.ok41

err40:                                            ; preds = %err.ok39
  ret %MultiMap zeroinitializer

err.ok41:                                         ; preds = %err.ok39
  br label %if.end33

err42:                                            ; preds = %if.end33
  ret %MultiMap zeroinitializer

err.ok43:                                         ; preds = %if.end33
  br label %if.end16

err44:                                            ; preds = %if.end16
  ret %MultiMap zeroinitializer

err.ok45:                                         ; preds = %if.end16
  br label %if.end

err46:                                            ; preds = %if.end
  ret %MultiMap zeroinitializer

err.ok47:                                         ; preds = %if.end
  %102 = load i64, ptr %i, align 4
  %103 = add i64 %102, 1
  store i64 %103, ptr %i, align 4
  %104 = call i32 @sere_has_error()
  %105 = icmp ne i32 %104, 0
  br i1 %105, label %err48, label %err.ok49

err48:                                            ; preds = %err.ok47
  ret %MultiMap zeroinitializer

err.ok49:                                         ; preds = %err.ok47
  br label %while.cond

err50:                                            ; preds = %while.end
  ret %MultiMap zeroinitializer

err.ok51:                                         ; preds = %while.end
  ret %MultiMap %27
}

define { ptr, i64 } @wsgi_headers_block(%MultiMap %headers) {
entry:
  %headers1 = alloca %MultiMap, align 8
  store %MultiMap %headers, ptr %headers1, align 8
  %out = alloca { ptr, i64 }, align 8
  store { ptr, i64 } { ptr @5, i64 0 }, ptr %out, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok8, %entry
  %0 = load i64, ptr %i, align 4
  %1 = getelementptr inbounds nuw %MultiMap, ptr %headers1, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = call i64 @sere_list_len(ptr %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

while.body:                                       ; preds = %err.ok
  %6 = load { ptr, i64 }, ptr %out, align 8
  %7 = getelementptr inbounds nuw %MultiMap, ptr %headers1, i32 0, i32 1
  %8 = load ptr, ptr %7, align 8
  %9 = load i64, ptr %i, align 4
  %10 = call ptr @sere_list_item(ptr %8, i64 %9)
  %11 = load { ptr, i64 }, ptr %10, align 8
  %cat.len = alloca i64, align 8
  %12 = extractvalue { ptr, i64 } %6, 0
  %13 = extractvalue { ptr, i64 } %6, 1
  %14 = extractvalue { ptr, i64 } %11, 0
  %15 = extractvalue { ptr, i64 } %11, 1
  %16 = call ptr @sere_str_concat_data(ptr %12, i64 %13, ptr %14, i64 %15, ptr %cat.len)
  %17 = load i64, ptr %cat.len, align 4
  %18 = insertvalue { ptr, i64 } undef, ptr %16, 0
  %19 = insertvalue { ptr, i64 } %18, i64 %17, 1
  %cat.len2 = alloca i64, align 8
  %20 = extractvalue { ptr, i64 } %19, 0
  %21 = extractvalue { ptr, i64 } %19, 1
  %22 = call ptr @sere_str_concat_data(ptr %20, i64 %21, ptr @6, i64 2, ptr %cat.len2)
  %23 = load i64, ptr %cat.len2, align 4
  %24 = insertvalue { ptr, i64 } undef, ptr %22, 0
  %25 = insertvalue { ptr, i64 } %24, i64 %23, 1
  %26 = getelementptr inbounds nuw %MultiMap, ptr %headers1, i32 0, i32 2
  %27 = load ptr, ptr %26, align 8
  %28 = load i64, ptr %i, align 4
  %29 = call ptr @sere_list_item(ptr %27, i64 %28)
  %30 = load { ptr, i64 }, ptr %29, align 8
  %cat.len3 = alloca i64, align 8
  %31 = extractvalue { ptr, i64 } %25, 0
  %32 = extractvalue { ptr, i64 } %25, 1
  %33 = extractvalue { ptr, i64 } %30, 0
  %34 = extractvalue { ptr, i64 } %30, 1
  %35 = call ptr @sere_str_concat_data(ptr %31, i64 %32, ptr %33, i64 %34, ptr %cat.len3)
  %36 = load i64, ptr %cat.len3, align 4
  %37 = insertvalue { ptr, i64 } undef, ptr %35, 0
  %38 = insertvalue { ptr, i64 } %37, i64 %36, 1
  %cat.len4 = alloca i64, align 8
  %39 = extractvalue { ptr, i64 } %38, 0
  %40 = extractvalue { ptr, i64 } %38, 1
  %41 = call ptr @sere_str_concat_data(ptr %39, i64 %40, ptr @7, i64 2, ptr %cat.len4)
  %42 = load i64, ptr %cat.len4, align 4
  %43 = insertvalue { ptr, i64 } undef, ptr %41, 0
  %44 = insertvalue { ptr, i64 } %43, i64 %42, 1
  store { ptr, i64 } %44, ptr %out, align 8
  %45 = call i32 @sere_has_error()
  %46 = icmp ne i32 %45, 0
  br i1 %46, label %err5, label %err.ok6

while.end:                                        ; preds = %err.ok
  %47 = load { ptr, i64 }, ptr %out, align 8
  %48 = call i32 @sere_has_error()
  %49 = icmp ne i32 %48, 0
  br i1 %49, label %err9, label %err.ok10

err:                                              ; preds = %while.cond
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %while.cond
  %50 = icmp slt i64 %0, %3
  br i1 %50, label %while.body, label %while.end

err5:                                             ; preds = %while.body
  ret { ptr, i64 } zeroinitializer

err.ok6:                                          ; preds = %while.body
  %51 = load i64, ptr %i, align 4
  %52 = add i64 %51, 1
  store i64 %52, ptr %i, align 4
  %53 = call i32 @sere_has_error()
  %54 = icmp ne i32 %53, 0
  br i1 %54, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret { ptr, i64 } zeroinitializer

err.ok8:                                          ; preds = %err.ok6
  br label %while.cond

err9:                                             ; preds = %while.end
  ret { ptr, i64 } zeroinitializer

err.ok10:                                         ; preds = %while.end
  ret { ptr, i64 } %47
}

define { ptr, i64 } @wsgi_status_text(i32 %status) {
entry:
  %status1 = alloca i32, align 4
  store i32 %status, ptr %status1, align 4
  %0 = load i32, ptr %status1, align 4
  %1 = icmp eq i32 %0, 100
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %2 = load i32, ptr %status1, align 4
  %3 = icmp eq i32 %2, 101
  br i1 %3, label %if.then3, label %if.next4

if.then:                                          ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %if.then
  ret { ptr, i64 } { ptr @8, i64 8 }

if.end2:                                          ; preds = %if.next4
  %6 = load i32, ptr %status1, align 4
  %7 = icmp eq i32 %6, 200
  br i1 %7, label %if.then8, label %if.next9

if.then3:                                         ; preds = %if.end
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err5, label %err.ok6

if.next4:                                         ; preds = %if.end
  br label %if.end2

err5:                                             ; preds = %if.then3
  ret { ptr, i64 } zeroinitializer

err.ok6:                                          ; preds = %if.then3
  ret { ptr, i64 } { ptr @9, i64 19 }

if.end7:                                          ; preds = %if.next9
  %10 = load i32, ptr %status1, align 4
  %11 = icmp eq i32 %10, 201
  br i1 %11, label %if.then13, label %if.next14

if.then8:                                         ; preds = %if.end2
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err10, label %err.ok11

if.next9:                                         ; preds = %if.end2
  br label %if.end7

err10:                                            ; preds = %if.then8
  ret { ptr, i64 } zeroinitializer

err.ok11:                                         ; preds = %if.then8
  ret { ptr, i64 } { ptr @10, i64 2 }

if.end12:                                         ; preds = %if.next14
  %14 = load i32, ptr %status1, align 4
  %15 = icmp eq i32 %14, 202
  br i1 %15, label %if.then18, label %if.next19

if.then13:                                        ; preds = %if.end7
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err15, label %err.ok16

if.next14:                                        ; preds = %if.end7
  br label %if.end12

err15:                                            ; preds = %if.then13
  ret { ptr, i64 } zeroinitializer

err.ok16:                                         ; preds = %if.then13
  ret { ptr, i64 } { ptr @11, i64 7 }

if.end17:                                         ; preds = %if.next19
  %18 = load i32, ptr %status1, align 4
  %19 = icmp eq i32 %18, 204
  br i1 %19, label %if.then23, label %if.next24

if.then18:                                        ; preds = %if.end12
  %20 = call i32 @sere_has_error()
  %21 = icmp ne i32 %20, 0
  br i1 %21, label %err20, label %err.ok21

if.next19:                                        ; preds = %if.end12
  br label %if.end17

err20:                                            ; preds = %if.then18
  ret { ptr, i64 } zeroinitializer

err.ok21:                                         ; preds = %if.then18
  ret { ptr, i64 } { ptr @12, i64 8 }

if.end22:                                         ; preds = %if.next24
  %22 = load i32, ptr %status1, align 4
  %23 = icmp eq i32 %22, 205
  br i1 %23, label %if.then28, label %if.next29

if.then23:                                        ; preds = %if.end17
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err25, label %err.ok26

if.next24:                                        ; preds = %if.end17
  br label %if.end22

err25:                                            ; preds = %if.then23
  ret { ptr, i64 } zeroinitializer

err.ok26:                                         ; preds = %if.then23
  ret { ptr, i64 } { ptr @13, i64 10 }

if.end27:                                         ; preds = %if.next29
  %26 = load i32, ptr %status1, align 4
  %27 = icmp eq i32 %26, 206
  br i1 %27, label %if.then33, label %if.next34

if.then28:                                        ; preds = %if.end22
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err30, label %err.ok31

if.next29:                                        ; preds = %if.end22
  br label %if.end27

err30:                                            ; preds = %if.then28
  ret { ptr, i64 } zeroinitializer

err.ok31:                                         ; preds = %if.then28
  ret { ptr, i64 } { ptr @14, i64 13 }

if.end32:                                         ; preds = %if.next34
  %30 = load i32, ptr %status1, align 4
  %31 = icmp eq i32 %30, 301
  br i1 %31, label %if.then38, label %if.next39

if.then33:                                        ; preds = %if.end27
  %32 = call i32 @sere_has_error()
  %33 = icmp ne i32 %32, 0
  br i1 %33, label %err35, label %err.ok36

if.next34:                                        ; preds = %if.end27
  br label %if.end32

err35:                                            ; preds = %if.then33
  ret { ptr, i64 } zeroinitializer

err.ok36:                                         ; preds = %if.then33
  ret { ptr, i64 } { ptr @15, i64 15 }

if.end37:                                         ; preds = %if.next39
  %34 = load i32, ptr %status1, align 4
  %35 = icmp eq i32 %34, 302
  br i1 %35, label %if.then43, label %if.next44

if.then38:                                        ; preds = %if.end32
  %36 = call i32 @sere_has_error()
  %37 = icmp ne i32 %36, 0
  br i1 %37, label %err40, label %err.ok41

if.next39:                                        ; preds = %if.end32
  br label %if.end37

err40:                                            ; preds = %if.then38
  ret { ptr, i64 } zeroinitializer

err.ok41:                                         ; preds = %if.then38
  ret { ptr, i64 } { ptr @16, i64 17 }

if.end42:                                         ; preds = %if.next44
  %38 = load i32, ptr %status1, align 4
  %39 = icmp eq i32 %38, 303
  br i1 %39, label %if.then48, label %if.next49

if.then43:                                        ; preds = %if.end37
  %40 = call i32 @sere_has_error()
  %41 = icmp ne i32 %40, 0
  br i1 %41, label %err45, label %err.ok46

if.next44:                                        ; preds = %if.end37
  br label %if.end42

err45:                                            ; preds = %if.then43
  ret { ptr, i64 } zeroinitializer

err.ok46:                                         ; preds = %if.then43
  ret { ptr, i64 } { ptr @17, i64 5 }

if.end47:                                         ; preds = %if.next49
  %42 = load i32, ptr %status1, align 4
  %43 = icmp eq i32 %42, 304
  br i1 %43, label %if.then53, label %if.next54

if.then48:                                        ; preds = %if.end42
  %44 = call i32 @sere_has_error()
  %45 = icmp ne i32 %44, 0
  br i1 %45, label %err50, label %err.ok51

if.next49:                                        ; preds = %if.end42
  br label %if.end47

err50:                                            ; preds = %if.then48
  ret { ptr, i64 } zeroinitializer

err.ok51:                                         ; preds = %if.then48
  ret { ptr, i64 } { ptr @18, i64 9 }

if.end52:                                         ; preds = %if.next54
  %46 = load i32, ptr %status1, align 4
  %47 = icmp eq i32 %46, 307
  br i1 %47, label %if.then58, label %if.next59

if.then53:                                        ; preds = %if.end47
  %48 = call i32 @sere_has_error()
  %49 = icmp ne i32 %48, 0
  br i1 %49, label %err55, label %err.ok56

if.next54:                                        ; preds = %if.end47
  br label %if.end52

err55:                                            ; preds = %if.then53
  ret { ptr, i64 } zeroinitializer

err.ok56:                                         ; preds = %if.then53
  ret { ptr, i64 } { ptr @19, i64 12 }

if.end57:                                         ; preds = %if.next59
  %50 = load i32, ptr %status1, align 4
  %51 = icmp eq i32 %50, 308
  br i1 %51, label %if.then63, label %if.next64

if.then58:                                        ; preds = %if.end52
  %52 = call i32 @sere_has_error()
  %53 = icmp ne i32 %52, 0
  br i1 %53, label %err60, label %err.ok61

if.next59:                                        ; preds = %if.end52
  br label %if.end57

err60:                                            ; preds = %if.then58
  ret { ptr, i64 } zeroinitializer

err.ok61:                                         ; preds = %if.then58
  ret { ptr, i64 } { ptr @20, i64 18 }

if.end62:                                         ; preds = %if.next64
  %54 = load i32, ptr %status1, align 4
  %55 = icmp eq i32 %54, 400
  br i1 %55, label %if.then68, label %if.next69

if.then63:                                        ; preds = %if.end57
  %56 = call i32 @sere_has_error()
  %57 = icmp ne i32 %56, 0
  br i1 %57, label %err65, label %err.ok66

if.next64:                                        ; preds = %if.end57
  br label %if.end62

err65:                                            ; preds = %if.then63
  ret { ptr, i64 } zeroinitializer

err.ok66:                                         ; preds = %if.then63
  ret { ptr, i64 } { ptr @21, i64 18 }

if.end67:                                         ; preds = %if.next69
  %58 = load i32, ptr %status1, align 4
  %59 = icmp eq i32 %58, 401
  br i1 %59, label %if.then73, label %if.next74

if.then68:                                        ; preds = %if.end62
  %60 = call i32 @sere_has_error()
  %61 = icmp ne i32 %60, 0
  br i1 %61, label %err70, label %err.ok71

if.next69:                                        ; preds = %if.end62
  br label %if.end67

err70:                                            ; preds = %if.then68
  ret { ptr, i64 } zeroinitializer

err.ok71:                                         ; preds = %if.then68
  ret { ptr, i64 } { ptr @22, i64 11 }

if.end72:                                         ; preds = %if.next74
  %62 = load i32, ptr %status1, align 4
  %63 = icmp eq i32 %62, 403
  br i1 %63, label %if.then78, label %if.next79

if.then73:                                        ; preds = %if.end67
  %64 = call i32 @sere_has_error()
  %65 = icmp ne i32 %64, 0
  br i1 %65, label %err75, label %err.ok76

if.next74:                                        ; preds = %if.end67
  br label %if.end72

err75:                                            ; preds = %if.then73
  ret { ptr, i64 } zeroinitializer

err.ok76:                                         ; preds = %if.then73
  ret { ptr, i64 } { ptr @23, i64 12 }

if.end77:                                         ; preds = %if.next79
  %66 = load i32, ptr %status1, align 4
  %67 = icmp eq i32 %66, 404
  br i1 %67, label %if.then83, label %if.next84

if.then78:                                        ; preds = %if.end72
  %68 = call i32 @sere_has_error()
  %69 = icmp ne i32 %68, 0
  br i1 %69, label %err80, label %err.ok81

if.next79:                                        ; preds = %if.end72
  br label %if.end77

err80:                                            ; preds = %if.then78
  ret { ptr, i64 } zeroinitializer

err.ok81:                                         ; preds = %if.then78
  ret { ptr, i64 } { ptr @24, i64 9 }

if.end82:                                         ; preds = %if.next84
  %70 = load i32, ptr %status1, align 4
  %71 = icmp eq i32 %70, 405
  br i1 %71, label %if.then88, label %if.next89

if.then83:                                        ; preds = %if.end77
  %72 = call i32 @sere_has_error()
  %73 = icmp ne i32 %72, 0
  br i1 %73, label %err85, label %err.ok86

if.next84:                                        ; preds = %if.end77
  br label %if.end82

err85:                                            ; preds = %if.then83
  ret { ptr, i64 } zeroinitializer

err.ok86:                                         ; preds = %if.then83
  ret { ptr, i64 } { ptr @25, i64 9 }

if.end87:                                         ; preds = %if.next89
  %74 = load i32, ptr %status1, align 4
  %75 = icmp eq i32 %74, 406
  br i1 %75, label %if.then93, label %if.next94

if.then88:                                        ; preds = %if.end82
  %76 = call i32 @sere_has_error()
  %77 = icmp ne i32 %76, 0
  br i1 %77, label %err90, label %err.ok91

if.next89:                                        ; preds = %if.end82
  br label %if.end87

err90:                                            ; preds = %if.then88
  ret { ptr, i64 } zeroinitializer

err.ok91:                                         ; preds = %if.then88
  ret { ptr, i64 } { ptr @26, i64 18 }

if.end92:                                         ; preds = %if.next94
  %78 = load i32, ptr %status1, align 4
  %79 = icmp eq i32 %78, 408
  br i1 %79, label %if.then98, label %if.next99

if.then93:                                        ; preds = %if.end87
  %80 = call i32 @sere_has_error()
  %81 = icmp ne i32 %80, 0
  br i1 %81, label %err95, label %err.ok96

if.next94:                                        ; preds = %if.end87
  br label %if.end92

err95:                                            ; preds = %if.then93
  ret { ptr, i64 } zeroinitializer

err.ok96:                                         ; preds = %if.then93
  ret { ptr, i64 } { ptr @27, i64 14 }

if.end97:                                         ; preds = %if.next99
  %82 = load i32, ptr %status1, align 4
  %83 = icmp eq i32 %82, 409
  br i1 %83, label %if.then103, label %if.next104

if.then98:                                        ; preds = %if.end92
  %84 = call i32 @sere_has_error()
  %85 = icmp ne i32 %84, 0
  br i1 %85, label %err100, label %err.ok101

if.next99:                                        ; preds = %if.end92
  br label %if.end97

err100:                                           ; preds = %if.then98
  ret { ptr, i64 } zeroinitializer

err.ok101:                                        ; preds = %if.then98
  ret { ptr, i64 } { ptr @28, i64 15 }

if.end102:                                        ; preds = %if.next104
  %86 = load i32, ptr %status1, align 4
  %87 = icmp eq i32 %86, 410
  br i1 %87, label %if.then108, label %if.next109

if.then103:                                       ; preds = %if.end97
  %88 = call i32 @sere_has_error()
  %89 = icmp ne i32 %88, 0
  br i1 %89, label %err105, label %err.ok106

if.next104:                                       ; preds = %if.end97
  br label %if.end102

err105:                                           ; preds = %if.then103
  ret { ptr, i64 } zeroinitializer

err.ok106:                                        ; preds = %if.then103
  ret { ptr, i64 } { ptr @29, i64 8 }

if.end107:                                        ; preds = %if.next109
  %90 = load i32, ptr %status1, align 4
  %91 = icmp eq i32 %90, 413
  br i1 %91, label %if.then113, label %if.next114

if.then108:                                       ; preds = %if.end102
  %92 = call i32 @sere_has_error()
  %93 = icmp ne i32 %92, 0
  br i1 %93, label %err110, label %err.ok111

if.next109:                                       ; preds = %if.end102
  br label %if.end107

err110:                                           ; preds = %if.then108
  ret { ptr, i64 } zeroinitializer

err.ok111:                                        ; preds = %if.then108
  ret { ptr, i64 } { ptr @30, i64 4 }

if.end112:                                        ; preds = %if.next114
  %94 = load i32, ptr %status1, align 4
  %95 = icmp eq i32 %94, 415
  br i1 %95, label %if.then118, label %if.next119

if.then113:                                       ; preds = %if.end107
  %96 = call i32 @sere_has_error()
  %97 = icmp ne i32 %96, 0
  br i1 %97, label %err115, label %err.ok116

if.next114:                                       ; preds = %if.end107
  br label %if.end112

err115:                                           ; preds = %if.then113
  ret { ptr, i64 } zeroinitializer

err.ok116:                                        ; preds = %if.then113
  ret { ptr, i64 } { ptr @31, i64 17 }

if.end117:                                        ; preds = %if.next119
  %98 = load i32, ptr %status1, align 4
  %99 = icmp eq i32 %98, 418
  br i1 %99, label %if.then123, label %if.next124

if.then118:                                       ; preds = %if.end112
  %100 = call i32 @sere_has_error()
  %101 = icmp ne i32 %100, 0
  br i1 %101, label %err120, label %err.ok121

if.next119:                                       ; preds = %if.end112
  br label %if.end117

err120:                                           ; preds = %if.then118
  ret { ptr, i64 } zeroinitializer

err.ok121:                                        ; preds = %if.then118
  ret { ptr, i64 } { ptr @32, i64 22 }

if.end122:                                        ; preds = %if.next124
  %102 = load i32, ptr %status1, align 4
  %103 = icmp eq i32 %102, 422
  br i1 %103, label %if.then128, label %if.next129

if.then123:                                       ; preds = %if.end117
  %104 = call i32 @sere_has_error()
  %105 = icmp ne i32 %104, 0
  br i1 %105, label %err125, label %err.ok126

if.next124:                                       ; preds = %if.end117
  br label %if.end122

err125:                                           ; preds = %if.then123
  ret { ptr, i64 } zeroinitializer

err.ok126:                                        ; preds = %if.then123
  ret { ptr, i64 } { ptr @33, i64 12 }

if.end127:                                        ; preds = %if.next129
  %106 = load i32, ptr %status1, align 4
  %107 = icmp eq i32 %106, 429
  br i1 %107, label %if.then133, label %if.next134

if.then128:                                       ; preds = %if.end122
  %108 = call i32 @sere_has_error()
  %109 = icmp ne i32 %108, 0
  br i1 %109, label %err130, label %err.ok131

if.next129:                                       ; preds = %if.end122
  br label %if.end127

err130:                                           ; preds = %if.then128
  ret { ptr, i64 } zeroinitializer

err.ok131:                                        ; preds = %if.then128
  ret { ptr, i64 } { ptr @34, i64 20 }

if.end132:                                        ; preds = %if.next134
  %110 = load i32, ptr %status1, align 4
  %111 = icmp eq i32 %110, 500
  br i1 %111, label %if.then138, label %if.next139

if.then133:                                       ; preds = %if.end127
  %112 = call i32 @sere_has_error()
  %113 = icmp ne i32 %112, 0
  br i1 %113, label %err135, label %err.ok136

if.next134:                                       ; preds = %if.end127
  br label %if.end132

err135:                                           ; preds = %if.then133
  ret { ptr, i64 } zeroinitializer

err.ok136:                                        ; preds = %if.then133
  ret { ptr, i64 } { ptr @35, i64 17 }

if.end137:                                        ; preds = %if.next139
  %114 = load i32, ptr %status1, align 4
  %115 = icmp eq i32 %114, 501
  br i1 %115, label %if.then143, label %if.next144

if.then138:                                       ; preds = %if.end132
  %116 = call i32 @sere_has_error()
  %117 = icmp ne i32 %116, 0
  br i1 %117, label %err140, label %err.ok141

if.next139:                                       ; preds = %if.end132
  br label %if.end137

err140:                                           ; preds = %if.then138
  ret { ptr, i64 } zeroinitializer

err.ok141:                                        ; preds = %if.then138
  ret { ptr, i64 } { ptr @36, i64 21 }

if.end142:                                        ; preds = %if.next144
  %118 = load i32, ptr %status1, align 4
  %119 = icmp eq i32 %118, 502
  br i1 %119, label %if.then148, label %if.next149

if.then143:                                       ; preds = %if.end137
  %120 = call i32 @sere_has_error()
  %121 = icmp ne i32 %120, 0
  br i1 %121, label %err145, label %err.ok146

if.next144:                                       ; preds = %if.end137
  br label %if.end142

err145:                                           ; preds = %if.then143
  ret { ptr, i64 } zeroinitializer

err.ok146:                                        ; preds = %if.then143
  ret { ptr, i64 } { ptr @37, i64 15 }

if.end147:                                        ; preds = %if.next149
  %122 = load i32, ptr %status1, align 4
  %123 = icmp eq i32 %122, 503
  br i1 %123, label %if.then153, label %if.next154

if.then148:                                       ; preds = %if.end142
  %124 = call i32 @sere_has_error()
  %125 = icmp ne i32 %124, 0
  br i1 %125, label %err150, label %err.ok151

if.next149:                                       ; preds = %if.end142
  br label %if.end147

err150:                                           ; preds = %if.then148
  ret { ptr, i64 } zeroinitializer

err.ok151:                                        ; preds = %if.then148
  ret { ptr, i64 } { ptr @38, i64 11 }

if.end152:                                        ; preds = %if.next154
  %126 = load i32, ptr %status1, align 4
  %127 = icmp eq i32 %126, 504
  br i1 %127, label %if.then158, label %if.next159

if.then153:                                       ; preds = %if.end147
  %128 = call i32 @sere_has_error()
  %129 = icmp ne i32 %128, 0
  br i1 %129, label %err155, label %err.ok156

if.next154:                                       ; preds = %if.end147
  br label %if.end152

err155:                                           ; preds = %if.then153
  ret { ptr, i64 } zeroinitializer

err.ok156:                                        ; preds = %if.then153
  ret { ptr, i64 } { ptr @39, i64 19 }

if.end157:                                        ; preds = %if.next159
  %130 = load i32, ptr %status1, align 4
  %131 = icmp eq i32 %130, 505
  br i1 %131, label %if.then163, label %if.next164

if.then158:                                       ; preds = %if.end152
  %132 = call i32 @sere_has_error()
  %133 = icmp ne i32 %132, 0
  br i1 %133, label %err160, label %err.ok161

if.next159:                                       ; preds = %if.end152
  br label %if.end157

err160:                                           ; preds = %if.then158
  ret { ptr, i64 } zeroinitializer

err.ok161:                                        ; preds = %if.then158
  ret { ptr, i64 } { ptr @40, i64 15 }

if.end162:                                        ; preds = %if.next164
  %134 = call i32 @sere_has_error()
  %135 = icmp ne i32 %134, 0
  br i1 %135, label %err167, label %err.ok168

if.then163:                                       ; preds = %if.end157
  %136 = call i32 @sere_has_error()
  %137 = icmp ne i32 %136, 0
  br i1 %137, label %err165, label %err.ok166

if.next164:                                       ; preds = %if.end157
  br label %if.end162

err165:                                           ; preds = %if.then163
  ret { ptr, i64 } zeroinitializer

err.ok166:                                        ; preds = %if.then163
  ret { ptr, i64 } { ptr @41, i64 26 }

err167:                                           ; preds = %if.end162
  ret { ptr, i64 } zeroinitializer

err.ok168:                                        ; preds = %if.end162
  ret { ptr, i64 } { ptr @42, i64 2 }
}

define { ptr, i64 } @wsgi_json_escape({ ptr, i64 } %value) {
entry:
  %value1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value1, align 8
  %out = alloca { ptr, i64 }, align 8
  store { ptr, i64 } { ptr @43, i64 0 }, ptr %out, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok32, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load { ptr, i64 }, ptr %value1, align 8
  %2 = extractvalue { ptr, i64 } %1, 1
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

while.body:                                       ; preds = %err.ok
  %ch = alloca { ptr, i64 }, align 8
  %5 = load { ptr, i64 }, ptr %value1, align 8
  %si.data = alloca ptr, align 8
  %si.len = alloca i64, align 8
  %6 = extractvalue { ptr, i64 } %5, 0
  %7 = extractvalue { ptr, i64 } %5, 1
  %8 = load i64, ptr %i, align 4
  call void @sere_str_index(ptr %6, i64 %7, i64 %8, ptr %si.data, ptr %si.len)
  %9 = load i64, ptr %si.len, align 4
  %10 = load ptr, ptr %si.data, align 8
  %11 = insertvalue { ptr, i64 } undef, ptr %10, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %9, 1
  store { ptr, i64 } %12, ptr %ch, align 8
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err2, label %err.ok3

while.end:                                        ; preds = %err.ok
  %15 = load { ptr, i64 }, ptr %out, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err33, label %err.ok34

err:                                              ; preds = %while.cond
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %while.cond
  %18 = icmp slt i64 %0, %2
  br i1 %18, label %while.body, label %while.end

err2:                                             ; preds = %while.body
  ret { ptr, i64 } zeroinitializer

err.ok3:                                          ; preds = %while.body
  %19 = load { ptr, i64 }, ptr %ch, align 8
  %20 = extractvalue { ptr, i64 } %19, 0
  %21 = extractvalue { ptr, i64 } %19, 1
  %22 = call i32 @sere_str_cmp(ptr %20, i64 %21, ptr @44, i64 1)
  %23 = icmp eq i32 %22, 0
  br i1 %23, label %if.then, label %if.next

if.end:                                           ; preds = %err.ok28, %err.ok25, %err.ok20, %err.ok15, %err.ok10, %err.ok5
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err29, label %err.ok30

if.then:                                          ; preds = %err.ok3
  %26 = load { ptr, i64 }, ptr %out, align 8
  %cat.len = alloca i64, align 8
  %27 = extractvalue { ptr, i64 } %26, 0
  %28 = extractvalue { ptr, i64 } %26, 1
  %29 = call ptr @sere_str_concat_data(ptr %27, i64 %28, ptr @45, i64 2, ptr %cat.len)
  %30 = load i64, ptr %cat.len, align 4
  %31 = insertvalue { ptr, i64 } undef, ptr %29, 0
  %32 = insertvalue { ptr, i64 } %31, i64 %30, 1
  store { ptr, i64 } %32, ptr %out, align 8
  %33 = call i32 @sere_has_error()
  %34 = icmp ne i32 %33, 0
  br i1 %34, label %err4, label %err.ok5

if.next:                                          ; preds = %err.ok3
  %35 = load { ptr, i64 }, ptr %ch, align 8
  %36 = extractvalue { ptr, i64 } %35, 0
  %37 = extractvalue { ptr, i64 } %35, 1
  %38 = call i32 @sere_str_cmp(ptr %36, i64 %37, ptr @46, i64 1)
  %39 = icmp eq i32 %38, 0
  br i1 %39, label %if.then6, label %if.next7

err4:                                             ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok5:                                          ; preds = %if.then
  br label %if.end

if.then6:                                         ; preds = %if.next
  %40 = load { ptr, i64 }, ptr %out, align 8
  %cat.len8 = alloca i64, align 8
  %41 = extractvalue { ptr, i64 } %40, 0
  %42 = extractvalue { ptr, i64 } %40, 1
  %43 = call ptr @sere_str_concat_data(ptr %41, i64 %42, ptr @47, i64 2, ptr %cat.len8)
  %44 = load i64, ptr %cat.len8, align 4
  %45 = insertvalue { ptr, i64 } undef, ptr %43, 0
  %46 = insertvalue { ptr, i64 } %45, i64 %44, 1
  store { ptr, i64 } %46, ptr %out, align 8
  %47 = call i32 @sere_has_error()
  %48 = icmp ne i32 %47, 0
  br i1 %48, label %err9, label %err.ok10

if.next7:                                         ; preds = %if.next
  %49 = load { ptr, i64 }, ptr %ch, align 8
  %50 = extractvalue { ptr, i64 } %49, 0
  %51 = extractvalue { ptr, i64 } %49, 1
  %52 = call i32 @sere_str_cmp(ptr %50, i64 %51, ptr @48, i64 1)
  %53 = icmp eq i32 %52, 0
  br i1 %53, label %if.then11, label %if.next12

err9:                                             ; preds = %if.then6
  ret { ptr, i64 } zeroinitializer

err.ok10:                                         ; preds = %if.then6
  br label %if.end

if.then11:                                        ; preds = %if.next7
  %54 = load { ptr, i64 }, ptr %out, align 8
  %cat.len13 = alloca i64, align 8
  %55 = extractvalue { ptr, i64 } %54, 0
  %56 = extractvalue { ptr, i64 } %54, 1
  %57 = call ptr @sere_str_concat_data(ptr %55, i64 %56, ptr @49, i64 2, ptr %cat.len13)
  %58 = load i64, ptr %cat.len13, align 4
  %59 = insertvalue { ptr, i64 } undef, ptr %57, 0
  %60 = insertvalue { ptr, i64 } %59, i64 %58, 1
  store { ptr, i64 } %60, ptr %out, align 8
  %61 = call i32 @sere_has_error()
  %62 = icmp ne i32 %61, 0
  br i1 %62, label %err14, label %err.ok15

if.next12:                                        ; preds = %if.next7
  %63 = load { ptr, i64 }, ptr %ch, align 8
  %64 = extractvalue { ptr, i64 } %63, 0
  %65 = extractvalue { ptr, i64 } %63, 1
  %66 = call i32 @sere_str_cmp(ptr %64, i64 %65, ptr @50, i64 1)
  %67 = icmp eq i32 %66, 0
  br i1 %67, label %if.then16, label %if.next17

err14:                                            ; preds = %if.then11
  ret { ptr, i64 } zeroinitializer

err.ok15:                                         ; preds = %if.then11
  br label %if.end

if.then16:                                        ; preds = %if.next12
  %68 = load { ptr, i64 }, ptr %out, align 8
  %cat.len18 = alloca i64, align 8
  %69 = extractvalue { ptr, i64 } %68, 0
  %70 = extractvalue { ptr, i64 } %68, 1
  %71 = call ptr @sere_str_concat_data(ptr %69, i64 %70, ptr @51, i64 2, ptr %cat.len18)
  %72 = load i64, ptr %cat.len18, align 4
  %73 = insertvalue { ptr, i64 } undef, ptr %71, 0
  %74 = insertvalue { ptr, i64 } %73, i64 %72, 1
  store { ptr, i64 } %74, ptr %out, align 8
  %75 = call i32 @sere_has_error()
  %76 = icmp ne i32 %75, 0
  br i1 %76, label %err19, label %err.ok20

if.next17:                                        ; preds = %if.next12
  %77 = load { ptr, i64 }, ptr %ch, align 8
  %78 = extractvalue { ptr, i64 } %77, 0
  %79 = extractvalue { ptr, i64 } %77, 1
  %80 = call i32 @sere_str_cmp(ptr %78, i64 %79, ptr @52, i64 1)
  %81 = icmp eq i32 %80, 0
  br i1 %81, label %if.then21, label %if.next22

err19:                                            ; preds = %if.then16
  ret { ptr, i64 } zeroinitializer

err.ok20:                                         ; preds = %if.then16
  br label %if.end

if.then21:                                        ; preds = %if.next17
  %82 = load { ptr, i64 }, ptr %out, align 8
  %cat.len23 = alloca i64, align 8
  %83 = extractvalue { ptr, i64 } %82, 0
  %84 = extractvalue { ptr, i64 } %82, 1
  %85 = call ptr @sere_str_concat_data(ptr %83, i64 %84, ptr @53, i64 2, ptr %cat.len23)
  %86 = load i64, ptr %cat.len23, align 4
  %87 = insertvalue { ptr, i64 } undef, ptr %85, 0
  %88 = insertvalue { ptr, i64 } %87, i64 %86, 1
  store { ptr, i64 } %88, ptr %out, align 8
  %89 = call i32 @sere_has_error()
  %90 = icmp ne i32 %89, 0
  br i1 %90, label %err24, label %err.ok25

if.next22:                                        ; preds = %if.next17
  %91 = load { ptr, i64 }, ptr %out, align 8
  %92 = load { ptr, i64 }, ptr %ch, align 8
  %cat.len26 = alloca i64, align 8
  %93 = extractvalue { ptr, i64 } %91, 0
  %94 = extractvalue { ptr, i64 } %91, 1
  %95 = extractvalue { ptr, i64 } %92, 0
  %96 = extractvalue { ptr, i64 } %92, 1
  %97 = call ptr @sere_str_concat_data(ptr %93, i64 %94, ptr %95, i64 %96, ptr %cat.len26)
  %98 = load i64, ptr %cat.len26, align 4
  %99 = insertvalue { ptr, i64 } undef, ptr %97, 0
  %100 = insertvalue { ptr, i64 } %99, i64 %98, 1
  store { ptr, i64 } %100, ptr %out, align 8
  %101 = call i32 @sere_has_error()
  %102 = icmp ne i32 %101, 0
  br i1 %102, label %err27, label %err.ok28

err24:                                            ; preds = %if.then21
  ret { ptr, i64 } zeroinitializer

err.ok25:                                         ; preds = %if.then21
  br label %if.end

err27:                                            ; preds = %if.next22
  ret { ptr, i64 } zeroinitializer

err.ok28:                                         ; preds = %if.next22
  br label %if.end

err29:                                            ; preds = %if.end
  ret { ptr, i64 } zeroinitializer

err.ok30:                                         ; preds = %if.end
  %103 = load i64, ptr %i, align 4
  %104 = add i64 %103, 1
  store i64 %104, ptr %i, align 4
  %105 = call i32 @sere_has_error()
  %106 = icmp ne i32 %105, 0
  br i1 %106, label %err31, label %err.ok32

err31:                                            ; preds = %err.ok30
  ret { ptr, i64 } zeroinitializer

err.ok32:                                         ; preds = %err.ok30
  br label %while.cond

err33:                                            ; preds = %while.end
  ret { ptr, i64 } zeroinitializer

err.ok34:                                         ; preds = %while.end
  ret { ptr, i64 } %15
}

define { ptr, i64 } @wsgi_json_string({ ptr, i64 } %value) {
entry:
  %value1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value1, align 8
  %0 = load { ptr, i64 }, ptr %value1, align 8
  %1 = call { ptr, i64 } @wsgi_json_escape({ ptr, i64 } %0)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  %cat.len = alloca i64, align 8
  %4 = extractvalue { ptr, i64 } %1, 0
  %5 = extractvalue { ptr, i64 } %1, 1
  %6 = call ptr @sere_str_concat_data(ptr @54, i64 1, ptr %4, i64 %5, ptr %cat.len)
  %7 = load i64, ptr %cat.len, align 4
  %8 = insertvalue { ptr, i64 } undef, ptr %6, 0
  %9 = insertvalue { ptr, i64 } %8, i64 %7, 1
  %cat.len2 = alloca i64, align 8
  %10 = extractvalue { ptr, i64 } %9, 0
  %11 = extractvalue { ptr, i64 } %9, 1
  %12 = call ptr @sere_str_concat_data(ptr %10, i64 %11, ptr @55, i64 1, ptr %cat.len2)
  %13 = load i64, ptr %cat.len2, align 4
  %14 = insertvalue { ptr, i64 } undef, ptr %12, 0
  %15 = insertvalue { ptr, i64 } %14, i64 %13, 1
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret { ptr, i64 } %15
}

define { ptr, i64 } @wsgi_content_type_for({ ptr, i64 } %path) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %lowered = alloca { ptr, i64 }, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %1, i64 %2, ptr %ext.str.data, ptr %ext.str.len)
  %3 = load i64, ptr %ext.str.len, align 4
  %4 = load ptr, ptr %ext.str.data, align 8
  %5 = insertvalue { ptr, i64 } undef, ptr %4, 0
  %6 = insertvalue { ptr, i64 } %5, i64 %3, 1
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  store { ptr, i64 } %6, ptr %lowered, align 8
  %9 = load { ptr, i64 }, ptr %lowered, align 8
  %10 = call i1 @string_ends_with({ ptr, i64 } %9, { ptr, i64 } { ptr @56, i64 5 })
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err2, label %err.ok3

if.end:                                           ; preds = %if.next
  %13 = load { ptr, i64 }, ptr %lowered, align 8
  %14 = call i1 @string_ends_with({ ptr, i64 } %13, { ptr, i64 } { ptr @59, i64 4 })
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err9, label %err.ok10

err2:                                             ; preds = %err.ok
  ret { ptr, i64 } zeroinitializer

err.ok3:                                          ; preds = %err.ok
  br i1 %10, label %log.end, label %log.rhs

log.rhs:                                          ; preds = %err.ok3
  %17 = load { ptr, i64 }, ptr %lowered, align 8
  %18 = call i1 @string_ends_with({ ptr, i64 } %17, { ptr, i64 } { ptr @57, i64 4 })
  %19 = call i32 @sere_has_error()
  %20 = icmp ne i32 %19, 0
  br i1 %20, label %err4, label %err.ok5

log.end:                                          ; preds = %err.ok5, %err.ok3
  %log.phi = phi i1 [ %10, %err.ok3 ], [ %18, %err.ok5 ]
  br i1 %log.phi, label %if.then, label %if.next

err4:                                             ; preds = %log.rhs
  ret { ptr, i64 } zeroinitializer

err.ok5:                                          ; preds = %log.rhs
  br label %log.end

if.then:                                          ; preds = %log.end
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err6, label %err.ok7

if.next:                                          ; preds = %log.end
  br label %if.end

err6:                                             ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok7:                                          ; preds = %if.then
  ret { ptr, i64 } { ptr @58, i64 24 }

if.end8:                                          ; preds = %if.next12
  %23 = load { ptr, i64 }, ptr %lowered, align 8
  %24 = call i1 @string_ends_with({ ptr, i64 } %23, { ptr, i64 } { ptr @61, i64 3 })
  %25 = call i32 @sere_has_error()
  %26 = icmp ne i32 %25, 0
  br i1 %26, label %err16, label %err.ok17

err9:                                             ; preds = %if.end
  ret { ptr, i64 } zeroinitializer

err.ok10:                                         ; preds = %if.end
  br i1 %14, label %if.then11, label %if.next12

if.then11:                                        ; preds = %err.ok10
  %27 = call i32 @sere_has_error()
  %28 = icmp ne i32 %27, 0
  br i1 %28, label %err13, label %err.ok14

if.next12:                                        ; preds = %err.ok10
  br label %if.end8

err13:                                            ; preds = %if.then11
  ret { ptr, i64 } zeroinitializer

err.ok14:                                         ; preds = %if.then11
  ret { ptr, i64 } { ptr @60, i64 23 }

if.end15:                                         ; preds = %if.next19
  %29 = load { ptr, i64 }, ptr %lowered, align 8
  %30 = call i1 @string_ends_with({ ptr, i64 } %29, { ptr, i64 } { ptr @63, i64 5 })
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err23, label %err.ok24

err16:                                            ; preds = %if.end8
  ret { ptr, i64 } zeroinitializer

err.ok17:                                         ; preds = %if.end8
  br i1 %24, label %if.then18, label %if.next19

if.then18:                                        ; preds = %err.ok17
  %33 = call i32 @sere_has_error()
  %34 = icmp ne i32 %33, 0
  br i1 %34, label %err20, label %err.ok21

if.next19:                                        ; preds = %err.ok17
  br label %if.end15

err20:                                            ; preds = %if.then18
  ret { ptr, i64 } zeroinitializer

err.ok21:                                         ; preds = %if.then18
  ret { ptr, i64 } { ptr @62, i64 37 }

if.end22:                                         ; preds = %if.next26
  %35 = load { ptr, i64 }, ptr %lowered, align 8
  %36 = call i1 @string_ends_with({ ptr, i64 } %35, { ptr, i64 } { ptr @65, i64 4 })
  %37 = call i32 @sere_has_error()
  %38 = icmp ne i32 %37, 0
  br i1 %38, label %err30, label %err.ok31

err23:                                            ; preds = %if.end15
  ret { ptr, i64 } zeroinitializer

err.ok24:                                         ; preds = %if.end15
  br i1 %30, label %if.then25, label %if.next26

if.then25:                                        ; preds = %err.ok24
  %39 = call i32 @sere_has_error()
  %40 = icmp ne i32 %39, 0
  br i1 %40, label %err27, label %err.ok28

if.next26:                                        ; preds = %err.ok24
  br label %if.end22

err27:                                            ; preds = %if.then25
  ret { ptr, i64 } zeroinitializer

err.ok28:                                         ; preds = %if.then25
  ret { ptr, i64 } { ptr @64, i64 31 }

if.end29:                                         ; preds = %if.next33
  %41 = load { ptr, i64 }, ptr %lowered, align 8
  %42 = call i1 @string_ends_with({ ptr, i64 } %41, { ptr, i64 } { ptr @67, i64 4 })
  %43 = call i32 @sere_has_error()
  %44 = icmp ne i32 %43, 0
  br i1 %44, label %err37, label %err.ok38

err30:                                            ; preds = %if.end22
  ret { ptr, i64 } zeroinitializer

err.ok31:                                         ; preds = %if.end22
  br i1 %36, label %if.then32, label %if.next33

if.then32:                                        ; preds = %err.ok31
  %45 = call i32 @sere_has_error()
  %46 = icmp ne i32 %45, 0
  br i1 %46, label %err34, label %err.ok35

if.next33:                                        ; preds = %err.ok31
  br label %if.end29

err34:                                            ; preds = %if.then32
  ret { ptr, i64 } zeroinitializer

err.ok35:                                         ; preds = %if.then32
  ret { ptr, i64 } { ptr @66, i64 25 }

if.end36:                                         ; preds = %if.next40
  %47 = load { ptr, i64 }, ptr %lowered, align 8
  %48 = call i1 @string_ends_with({ ptr, i64 } %47, { ptr, i64 } { ptr @69, i64 4 })
  %49 = call i32 @sere_has_error()
  %50 = icmp ne i32 %49, 0
  br i1 %50, label %err44, label %err.ok45

err37:                                            ; preds = %if.end29
  ret { ptr, i64 } zeroinitializer

err.ok38:                                         ; preds = %if.end29
  br i1 %42, label %if.then39, label %if.next40

if.then39:                                        ; preds = %err.ok38
  %51 = call i32 @sere_has_error()
  %52 = icmp ne i32 %51, 0
  br i1 %52, label %err41, label %err.ok42

if.next40:                                        ; preds = %err.ok38
  br label %if.end36

err41:                                            ; preds = %if.then39
  ret { ptr, i64 } zeroinitializer

err.ok42:                                         ; preds = %if.then39
  ret { ptr, i64 } { ptr @68, i64 13 }

if.end43:                                         ; preds = %if.next47
  %53 = load { ptr, i64 }, ptr %lowered, align 8
  %54 = call i1 @string_ends_with({ ptr, i64 } %53, { ptr, i64 } { ptr @71, i64 4 })
  %55 = call i32 @sere_has_error()
  %56 = icmp ne i32 %55, 0
  br i1 %56, label %err51, label %err.ok52

err44:                                            ; preds = %if.end36
  ret { ptr, i64 } zeroinitializer

err.ok45:                                         ; preds = %if.end36
  br i1 %48, label %if.then46, label %if.next47

if.then46:                                        ; preds = %err.ok45
  %57 = call i32 @sere_has_error()
  %58 = icmp ne i32 %57, 0
  br i1 %58, label %err48, label %err.ok49

if.next47:                                        ; preds = %err.ok45
  br label %if.end43

err48:                                            ; preds = %if.then46
  ret { ptr, i64 } zeroinitializer

err.ok49:                                         ; preds = %if.then46
  ret { ptr, i64 } { ptr @70, i64 30 }

if.end50:                                         ; preds = %if.next54
  %59 = call i32 @sere_has_error()
  %60 = icmp ne i32 %59, 0
  br i1 %60, label %err57, label %err.ok58

err51:                                            ; preds = %if.end43
  ret { ptr, i64 } zeroinitializer

err.ok52:                                         ; preds = %if.end43
  br i1 %54, label %if.then53, label %if.next54

if.then53:                                        ; preds = %err.ok52
  %61 = call i32 @sere_has_error()
  %62 = icmp ne i32 %61, 0
  br i1 %62, label %err55, label %err.ok56

if.next54:                                        ; preds = %err.ok52
  br label %if.end50

err55:                                            ; preds = %if.then53
  ret { ptr, i64 } zeroinitializer

err.ok56:                                         ; preds = %if.then53
  ret { ptr, i64 } { ptr @72, i64 23 }

err57:                                            ; preds = %if.end50
  ret { ptr, i64 } zeroinitializer

err.ok58:                                         ; preds = %if.end50
  ret { ptr, i64 } { ptr @73, i64 24 }
}

define void @wsgi_Environ___init__(ptr %self, { ptr, i64 } %method, { ptr, i64 } %path, { ptr, i64 } %body) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %method1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %method, ptr %method1, align 8
  %path2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path2, align 8
  %body3 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body3, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %method1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Environ, ptr %3, i32 0, i32 2
  %5 = load { ptr, i64 }, ptr %path2, align 8
  store { ptr, i64 } %5, ptr %4, align 8
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Environ, ptr %6, i32 0, i32 3
  store { ptr, i64 } { ptr @74, i64 0 }, ptr %7, align 8
  %8 = load ptr, ptr %self.slot, align 8
  %9 = getelementptr inbounds nuw %Environ, ptr %8, i32 0, i32 4
  store { ptr, i64 } { ptr @75, i64 8 }, ptr %9, align 8
  %10 = load ptr, ptr %self.slot, align 8
  %11 = getelementptr inbounds nuw %Environ, ptr %10, i32 0, i32 5
  %12 = load { ptr, i64 }, ptr %body3, align 8
  store { ptr, i64 } %12, ptr %11, align 8
  %13 = load ptr, ptr %self.slot, align 8
  %14 = getelementptr inbounds nuw %Environ, ptr %13, i32 0, i32 6
  %init.tmp = alloca %MultiMap, align 8
  store %MultiMap zeroinitializer, ptr %init.tmp, align 8
  %15 = getelementptr inbounds nuw %MultiMap, ptr %init.tmp, i32 0, i32 0
  store i32 1379361884, ptr %15, align 4
  call void @wsgi_MultiMap___init__(ptr %init.tmp)
  %16 = load %MultiMap, ptr %init.tmp, align 8
  %17 = call i32 @sere_has_error()
  %18 = icmp ne i32 %17, 0
  br i1 %18, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store %MultiMap %16, ptr %14, align 8
  %19 = load ptr, ptr %self.slot, align 8
  %20 = getelementptr inbounds nuw %Environ, ptr %19, i32 0, i32 7
  %init.tmp4 = alloca %MultiMap, align 8
  store %MultiMap zeroinitializer, ptr %init.tmp4, align 8
  %21 = getelementptr inbounds nuw %MultiMap, ptr %init.tmp4, i32 0, i32 0
  store i32 1379361884, ptr %21, align 4
  call void @wsgi_MultiMap___init__(ptr %init.tmp4)
  %22 = load %MultiMap, ptr %init.tmp4, align 8
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok
  ret void

err.ok6:                                          ; preds = %err.ok
  store %MultiMap %22, ptr %20, align 8
  ret void
}

define { ptr, i64 } @wsgi_Environ_header(ptr %self, { ptr, i64 } %name, { ptr, i64 } %fallback) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %fallback2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %fallback, ptr %fallback2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 6
  %2 = load { ptr, i64 }, ptr %name1, align 8
  %3 = load { ptr, i64 }, ptr %fallback2, align 8
  %4 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %1, { ptr, i64 } %2, { ptr, i64 } %3)
  %5 = call i32 @sere_has_error()
  %6 = icmp ne i32 %5, 0
  br i1 %6, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret { ptr, i64 } %4
}

define { ptr, i64 } @wsgi_Environ_param(ptr %self, { ptr, i64 } %name, { ptr, i64 } %fallback) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %fallback2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %fallback, ptr %fallback2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 7
  %2 = load { ptr, i64 }, ptr %name1, align 8
  %3 = load { ptr, i64 }, ptr %fallback2, align 8
  %4 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %1, { ptr, i64 } %2, { ptr, i64 } %3)
  %5 = call i32 @sere_has_error()
  %6 = icmp ne i32 %5, 0
  br i1 %6, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret { ptr, i64 } %4
}

define %MultiMap @wsgi_Environ_form(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 5
  %2 = load { ptr, i64 }, ptr %1, align 8
  %3 = call %MultiMap @wsgi_parse_query({ ptr, i64 } %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %MultiMap zeroinitializer

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret %MultiMap zeroinitializer

err.ok2:                                          ; preds = %err.ok
  ret %MultiMap %3
}

define i1 @wsgi_Environ_is_get(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %1, align 8
  %3 = extractvalue { ptr, i64 } %2, 0
  %4 = extractvalue { ptr, i64 } %2, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %3, i64 %4, ptr %ext.str.data, ptr %ext.str.len)
  %5 = load i64, ptr %ext.str.len, align 4
  %6 = load ptr, ptr %ext.str.data, align 8
  %7 = insertvalue { ptr, i64 } undef, ptr %6, 0
  %8 = insertvalue { ptr, i64 } %7, i64 %5, 1
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %11 = extractvalue { ptr, i64 } %8, 0
  %12 = extractvalue { ptr, i64 } %8, 1
  %13 = call i32 @sere_str_cmp(ptr %11, i64 %12, ptr @76, i64 3)
  %14 = icmp eq i32 %13, 0
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i1 false

err.ok2:                                          ; preds = %err.ok
  ret i1 %14
}

define i1 @wsgi_Environ_is_post(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %1, align 8
  %3 = extractvalue { ptr, i64 } %2, 0
  %4 = extractvalue { ptr, i64 } %2, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %3, i64 %4, ptr %ext.str.data, ptr %ext.str.len)
  %5 = load i64, ptr %ext.str.len, align 4
  %6 = load ptr, ptr %ext.str.data, align 8
  %7 = insertvalue { ptr, i64 } undef, ptr %6, 0
  %8 = insertvalue { ptr, i64 } %7, i64 %5, 1
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %11 = extractvalue { ptr, i64 } %8, 0
  %12 = extractvalue { ptr, i64 } %8, 1
  %13 = call i32 @sere_str_cmp(ptr %11, i64 %12, ptr @77, i64 4)
  %14 = icmp eq i32 %13, 0
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i1 false

err.ok2:                                          ; preds = %err.ok
  ret i1 %14
}

define i1 @wsgi_Environ_is_head(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Environ, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %1, align 8
  %3 = extractvalue { ptr, i64 } %2, 0
  %4 = extractvalue { ptr, i64 } %2, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %3, i64 %4, ptr %ext.str.data, ptr %ext.str.len)
  %5 = load i64, ptr %ext.str.len, align 4
  %6 = load ptr, ptr %ext.str.data, align 8
  %7 = insertvalue { ptr, i64 } undef, ptr %6, 0
  %8 = insertvalue { ptr, i64 } %7, i64 %5, 1
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %11 = extractvalue { ptr, i64 } %8, 0
  %12 = extractvalue { ptr, i64 } %8, 1
  %13 = call i32 @sere_str_cmp(ptr %11, i64 %12, ptr @78, i64 4)
  %14 = icmp eq i32 %13, 0
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i1 false

err.ok2:                                          ; preds = %err.ok
  ret i1 %14
}

define void @wsgi_Response___init__(ptr %self, i32 %status, { ptr, i64 } %content_type, { ptr, i64 } %body) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %status1 = alloca i32, align 4
  store i32 %status, ptr %status1, align 4
  %content_type2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %content_type, ptr %content_type2, align 8
  %body3 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body3, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Response, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %status1, align 4
  store i32 %2, ptr %1, align 4
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Response, ptr %3, i32 0, i32 2
  %5 = load { ptr, i64 }, ptr %content_type2, align 8
  store { ptr, i64 } %5, ptr %4, align 8
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Response, ptr %6, i32 0, i32 3
  %8 = load { ptr, i64 }, ptr %body3, align 8
  store { ptr, i64 } %8, ptr %7, align 8
  %9 = load ptr, ptr %self.slot, align 8
  %10 = getelementptr inbounds nuw %Response, ptr %9, i32 0, i32 4
  store { ptr, i64 } { ptr @79, i64 0 }, ptr %10, align 8
  %11 = load ptr, ptr %self.slot, align 8
  %12 = getelementptr inbounds nuw %Response, ptr %11, i32 0, i32 5
  %init.tmp = alloca %MultiMap, align 8
  store %MultiMap zeroinitializer, ptr %init.tmp, align 8
  %13 = getelementptr inbounds nuw %MultiMap, ptr %init.tmp, i32 0, i32 0
  store i32 1379361884, ptr %13, align 4
  call void @wsgi_MultiMap___init__(ptr %init.tmp)
  %14 = load %MultiMap, ptr %init.tmp, align 8
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store %MultiMap %14, ptr %12, align 8
  ret void
}

define void @wsgi_Response_set_header(ptr %self, { ptr, i64 } %name, { ptr, i64 } %value) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %value2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Response, ptr %0, i32 0, i32 5
  %2 = load { ptr, i64 }, ptr %name1, align 8
  %3 = load { ptr, i64 }, ptr %value2, align 8
  call void @wsgi_MultiMap_set(ptr %1, { ptr, i64 } %2, { ptr, i64 } %3)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Response_add_header(ptr %self, { ptr, i64 } %name, { ptr, i64 } %value) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %value2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Response, ptr %0, i32 0, i32 5
  %2 = load { ptr, i64 }, ptr %name1, align 8
  %3 = load { ptr, i64 }, ptr %value2, align 8
  call void @wsgi_MultiMap_add(ptr %1, { ptr, i64 } %2, { ptr, i64 } %3)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Response_set_cookie(ptr %self, { ptr, i64 } %name, { ptr, i64 } %value, { ptr, i64 } %path) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %name1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %name, ptr %name1, align 8
  %value2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %value, ptr %value2, align 8
  %path3 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path3, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Response, ptr %0, i32 0, i32 5
  %2 = load { ptr, i64 }, ptr %name1, align 8
  %cat.len = alloca i64, align 8
  %3 = extractvalue { ptr, i64 } %2, 0
  %4 = extractvalue { ptr, i64 } %2, 1
  %5 = call ptr @sere_str_concat_data(ptr %3, i64 %4, ptr @81, i64 1, ptr %cat.len)
  %6 = load i64, ptr %cat.len, align 4
  %7 = insertvalue { ptr, i64 } undef, ptr %5, 0
  %8 = insertvalue { ptr, i64 } %7, i64 %6, 1
  %9 = load { ptr, i64 }, ptr %value2, align 8
  %cat.len4 = alloca i64, align 8
  %10 = extractvalue { ptr, i64 } %8, 0
  %11 = extractvalue { ptr, i64 } %8, 1
  %12 = extractvalue { ptr, i64 } %9, 0
  %13 = extractvalue { ptr, i64 } %9, 1
  %14 = call ptr @sere_str_concat_data(ptr %10, i64 %11, ptr %12, i64 %13, ptr %cat.len4)
  %15 = load i64, ptr %cat.len4, align 4
  %16 = insertvalue { ptr, i64 } undef, ptr %14, 0
  %17 = insertvalue { ptr, i64 } %16, i64 %15, 1
  %cat.len5 = alloca i64, align 8
  %18 = extractvalue { ptr, i64 } %17, 0
  %19 = extractvalue { ptr, i64 } %17, 1
  %20 = call ptr @sere_str_concat_data(ptr %18, i64 %19, ptr @82, i64 7, ptr %cat.len5)
  %21 = load i64, ptr %cat.len5, align 4
  %22 = insertvalue { ptr, i64 } undef, ptr %20, 0
  %23 = insertvalue { ptr, i64 } %22, i64 %21, 1
  %24 = load { ptr, i64 }, ptr %path3, align 8
  %cat.len6 = alloca i64, align 8
  %25 = extractvalue { ptr, i64 } %23, 0
  %26 = extractvalue { ptr, i64 } %23, 1
  %27 = extractvalue { ptr, i64 } %24, 0
  %28 = extractvalue { ptr, i64 } %24, 1
  %29 = call ptr @sere_str_concat_data(ptr %25, i64 %26, ptr %27, i64 %28, ptr %cat.len6)
  %30 = load i64, ptr %cat.len6, align 4
  %31 = insertvalue { ptr, i64 } undef, ptr %29, 0
  %32 = insertvalue { ptr, i64 } %31, i64 %30, 1
  call void @wsgi_MultiMap_add(ptr %1, { ptr, i64 } { ptr @80, i64 10 }, { ptr, i64 } %32)
  %33 = call i32 @sere_has_error()
  %34 = icmp ne i32 %33, 0
  br i1 %34, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define { ptr, i64 } @wsgi_Response_status_line(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Response, ptr %0, i32 0, i32 4
  %2 = load { ptr, i64 }, ptr %1, align 8
  %3 = extractvalue { ptr, i64 } %2, 1
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.end:                                           ; preds = %if.next
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Response, ptr %6, i32 0, i32 1
  %8 = load i32, ptr %7, align 4
  %str.len6 = alloca i64, align 8
  %9 = call ptr @sere_str_i32_data(i32 %8, ptr %str.len6)
  %10 = load i64, ptr %str.len6, align 4
  %11 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %10, 1
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err7, label %err.ok8

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  %15 = icmp sgt i64 %3, 0
  br i1 %15, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok
  %16 = load ptr, ptr %self.slot, align 8
  %17 = getelementptr inbounds nuw %Response, ptr %16, i32 0, i32 1
  %18 = load i32, ptr %17, align 4
  %str.len = alloca i64, align 8
  %19 = call ptr @sere_str_i32_data(i32 %18, ptr %str.len)
  %20 = load i64, ptr %str.len, align 4
  %21 = insertvalue { ptr, i64 } undef, ptr %19, 0
  %22 = insertvalue { ptr, i64 } %21, i64 %20, 1
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err1, label %err.ok2

if.next:                                          ; preds = %err.ok
  br label %if.end

err1:                                             ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok2:                                          ; preds = %if.then
  %cat.len = alloca i64, align 8
  %25 = extractvalue { ptr, i64 } %22, 0
  %26 = extractvalue { ptr, i64 } %22, 1
  %27 = call ptr @sere_str_concat_data(ptr %25, i64 %26, ptr @83, i64 1, ptr %cat.len)
  %28 = load i64, ptr %cat.len, align 4
  %29 = insertvalue { ptr, i64 } undef, ptr %27, 0
  %30 = insertvalue { ptr, i64 } %29, i64 %28, 1
  %31 = load ptr, ptr %self.slot, align 8
  %32 = getelementptr inbounds nuw %Response, ptr %31, i32 0, i32 4
  %33 = load { ptr, i64 }, ptr %32, align 8
  %cat.len3 = alloca i64, align 8
  %34 = extractvalue { ptr, i64 } %30, 0
  %35 = extractvalue { ptr, i64 } %30, 1
  %36 = extractvalue { ptr, i64 } %33, 0
  %37 = extractvalue { ptr, i64 } %33, 1
  %38 = call ptr @sere_str_concat_data(ptr %34, i64 %35, ptr %36, i64 %37, ptr %cat.len3)
  %39 = load i64, ptr %cat.len3, align 4
  %40 = insertvalue { ptr, i64 } undef, ptr %38, 0
  %41 = insertvalue { ptr, i64 } %40, i64 %39, 1
  %42 = call i32 @sere_has_error()
  %43 = icmp ne i32 %42, 0
  br i1 %43, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok2
  ret { ptr, i64 } zeroinitializer

err.ok5:                                          ; preds = %err.ok2
  ret { ptr, i64 } %41

err7:                                             ; preds = %if.end
  ret { ptr, i64 } zeroinitializer

err.ok8:                                          ; preds = %if.end
  %cat.len9 = alloca i64, align 8
  %44 = extractvalue { ptr, i64 } %12, 0
  %45 = extractvalue { ptr, i64 } %12, 1
  %46 = call ptr @sere_str_concat_data(ptr %44, i64 %45, ptr @84, i64 1, ptr %cat.len9)
  %47 = load i64, ptr %cat.len9, align 4
  %48 = insertvalue { ptr, i64 } undef, ptr %46, 0
  %49 = insertvalue { ptr, i64 } %48, i64 %47, 1
  %50 = load ptr, ptr %self.slot, align 8
  %51 = getelementptr inbounds nuw %Response, ptr %50, i32 0, i32 1
  %52 = load i32, ptr %51, align 4
  %53 = call { ptr, i64 } @wsgi_status_text(i32 %52)
  %54 = call i32 @sere_has_error()
  %55 = icmp ne i32 %54, 0
  br i1 %55, label %err10, label %err.ok11

err10:                                            ; preds = %err.ok8
  ret { ptr, i64 } zeroinitializer

err.ok11:                                         ; preds = %err.ok8
  %cat.len12 = alloca i64, align 8
  %56 = extractvalue { ptr, i64 } %49, 0
  %57 = extractvalue { ptr, i64 } %49, 1
  %58 = extractvalue { ptr, i64 } %53, 0
  %59 = extractvalue { ptr, i64 } %53, 1
  %60 = call ptr @sere_str_concat_data(ptr %56, i64 %57, ptr %58, i64 %59, ptr %cat.len12)
  %61 = load i64, ptr %cat.len12, align 4
  %62 = insertvalue { ptr, i64 } undef, ptr %60, 0
  %63 = insertvalue { ptr, i64 } %62, i64 %61, 1
  %64 = call i32 @sere_has_error()
  %65 = icmp ne i32 %64, 0
  br i1 %65, label %err13, label %err.ok14

err13:                                            ; preds = %err.ok11
  ret { ptr, i64 } zeroinitializer

err.ok14:                                         ; preds = %err.ok11
  ret { ptr, i64 } %63
}

define %Response @wsgi_text({ ptr, i64 } %body, i32 %status) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %status2 = alloca i32, align 4
  store i32 %status, ptr %status2, align 4
  %init.tmp = alloca %Response, align 8
  store %Response zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Response, ptr %init.tmp, i32 0, i32 0
  store i32 1437172478, ptr %0, align 4
  %1 = load i32, ptr %status2, align 4
  %2 = load { ptr, i64 }, ptr %body1, align 8
  call void @wsgi_Response___init__(ptr %init.tmp, i32 %1, { ptr, i64 } { ptr @85, i64 25 }, { ptr, i64 } %2)
  %3 = load %Response, ptr %init.tmp, align 8
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret %Response %3
}

define %Response @wsgi_html({ ptr, i64 } %body, i32 %status) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %status2 = alloca i32, align 4
  store i32 %status, ptr %status2, align 4
  %init.tmp = alloca %Response, align 8
  store %Response zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Response, ptr %init.tmp, i32 0, i32 0
  store i32 1437172478, ptr %0, align 4
  %1 = load i32, ptr %status2, align 4
  %2 = load { ptr, i64 }, ptr %body1, align 8
  call void @wsgi_Response___init__(ptr %init.tmp, i32 %1, { ptr, i64 } { ptr @86, i64 24 }, { ptr, i64 } %2)
  %3 = load %Response, ptr %init.tmp, align 8
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret %Response %3
}

define %Response @wsgi_json({ ptr, i64 } %body, i32 %status) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %status2 = alloca i32, align 4
  store i32 %status, ptr %status2, align 4
  %init.tmp = alloca %Response, align 8
  store %Response zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Response, ptr %init.tmp, i32 0, i32 0
  store i32 1437172478, ptr %0, align 4
  %1 = load i32, ptr %status2, align 4
  %2 = load { ptr, i64 }, ptr %body1, align 8
  call void @wsgi_Response___init__(ptr %init.tmp, i32 %1, { ptr, i64 } { ptr @87, i64 31 }, { ptr, i64 } %2)
  %3 = load %Response, ptr %init.tmp, align 8
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret %Response %3
}

define %Response @wsgi_redirect({ ptr, i64 } %location, i32 %status) {
entry:
  %location1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %location, ptr %location1, align 8
  %status2 = alloca i32, align 4
  store i32 %status, ptr %status2, align 4
  %response = alloca %Response, align 8
  %init.tmp = alloca %Response, align 8
  store %Response zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Response, ptr %init.tmp, i32 0, i32 0
  store i32 1437172478, ptr %0, align 4
  %1 = load i32, ptr %status2, align 4
  call void @wsgi_Response___init__(ptr %init.tmp, i32 %1, { ptr, i64 } { ptr @88, i64 25 }, { ptr, i64 } { ptr @89, i64 0 })
  %2 = load %Response, ptr %init.tmp, align 8
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  store %Response %2, ptr %response, align 8
  %5 = load { ptr, i64 }, ptr %location1, align 8
  call void @wsgi_Response_set_header(ptr %response, { ptr, i64 } { ptr @90, i64 8 }, { ptr, i64 } %5)
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok4:                                          ; preds = %err.ok
  %8 = load %Response, ptr %response, align 8
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret %Response zeroinitializer

err.ok6:                                          ; preds = %err.ok4
  ret %Response %8
}

define %Response @wsgi_not_found({ ptr, i64 } %body) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %0 = load { ptr, i64 }, ptr %body1, align 8
  %1 = call %Response @wsgi_text({ ptr, i64 } %0, i32 404)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Response %1
}

define %Response @wsgi_forbidden({ ptr, i64 } %body) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %0 = load { ptr, i64 }, ptr %body1, align 8
  %1 = call %Response @wsgi_text({ ptr, i64 } %0, i32 403)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Response %1
}

define %Response @wsgi_bad_request({ ptr, i64 } %body) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %0 = load { ptr, i64 }, ptr %body1, align 8
  %1 = call %Response @wsgi_text({ ptr, i64 } %0, i32 400)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Response %1
}

define %Response @wsgi_method_not_allowed({ ptr, i64 } %body) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %0 = load { ptr, i64 }, ptr %body1, align 8
  %1 = call %Response @wsgi_text({ ptr, i64 } %0, i32 405)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Response %1
}

define %Response @wsgi_server_error({ ptr, i64 } %body) {
entry:
  %body1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %body, ptr %body1, align 8
  %0 = load { ptr, i64 }, ptr %body1, align 8
  %1 = call %Response @wsgi_text({ ptr, i64 } %0, i32 500)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Response %1
}

define %Response @wsgi_no_content() {
entry:
  %init.tmp = alloca %Response, align 8
  store %Response zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Response, ptr %init.tmp, i32 0, i32 0
  store i32 1437172478, ptr %0, align 4
  call void @wsgi_Response___init__(ptr %init.tmp, i32 204, { ptr, i64 } { ptr @91, i64 25 }, { ptr, i64 } { ptr @92, i64 0 })
  %1 = load %Response, ptr %init.tmp, align 8
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok2:                                          ; preds = %err.ok
  ret %Response %1
}

define void @wsgi_Route___init__(ptr %self, { ptr, i64 } %method, { ptr, i64 } %pattern, i32 %kind, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %method1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %method, ptr %method1, align 8
  %pattern2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern2, align 8
  %kind3 = alloca i32, align 4
  store i32 %kind, ptr %kind3, align 4
  %handler4 = alloca ptr, align 8
  store ptr %handler, ptr %handler4, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Route, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %method1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Route, ptr %3, i32 0, i32 2
  %5 = load { ptr, i64 }, ptr %pattern2, align 8
  store { ptr, i64 } %5, ptr %4, align 8
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Route, ptr %6, i32 0, i32 3
  %8 = load i32, ptr %kind3, align 4
  store i32 %8, ptr %7, align 4
  %9 = load ptr, ptr %self.slot, align 8
  %10 = getelementptr inbounds nuw %Route, ptr %9, i32 0, i32 4
  %11 = load ptr, ptr %handler4, align 8
  store ptr %11, ptr %10, align 8
  ret void
}

define i1 @wsgi__method_matches({ ptr, i64 } %allowed, { ptr, i64 } %actual) {
entry:
  %allowed1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %allowed, ptr %allowed1, align 8
  %actual2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %actual, ptr %actual2, align 8
  %0 = load { ptr, i64 }, ptr %allowed1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_str_cmp(ptr %1, i64 %2, ptr @93, i64 1)
  %4 = icmp eq i32 %3, 0
  br i1 %4, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %5 = load { ptr, i64 }, ptr %allowed1, align 8
  %6 = extractvalue { ptr, i64 } %5, 0
  %7 = extractvalue { ptr, i64 } %5, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_string_lower(ptr %6, i64 %7, ptr %ext.str.data, ptr %ext.str.len)
  %8 = load i64, ptr %ext.str.len, align 4
  %9 = load ptr, ptr %ext.str.data, align 8
  %10 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %11 = insertvalue { ptr, i64 } %10, i64 %8, 1
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err3, label %err.ok4

if.then:                                          ; preds = %entry
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret i1 false

err.ok:                                           ; preds = %if.then
  ret i1 true

err3:                                             ; preds = %if.end
  ret i1 false

err.ok4:                                          ; preds = %if.end
  %16 = load { ptr, i64 }, ptr %actual2, align 8
  %17 = extractvalue { ptr, i64 } %16, 0
  %18 = extractvalue { ptr, i64 } %16, 1
  %ext.str.data5 = alloca ptr, align 8
  %ext.str.len6 = alloca i64, align 8
  call void @sere_string_lower(ptr %17, i64 %18, ptr %ext.str.data5, ptr %ext.str.len6)
  %19 = load i64, ptr %ext.str.len6, align 4
  %20 = load ptr, ptr %ext.str.data5, align 8
  %21 = insertvalue { ptr, i64 } undef, ptr %20, 0
  %22 = insertvalue { ptr, i64 } %21, i64 %19, 1
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok4
  ret i1 false

err.ok8:                                          ; preds = %err.ok4
  %25 = extractvalue { ptr, i64 } %11, 0
  %26 = extractvalue { ptr, i64 } %11, 1
  %27 = extractvalue { ptr, i64 } %22, 0
  %28 = extractvalue { ptr, i64 } %22, 1
  %29 = call i32 @sere_str_cmp(ptr %25, i64 %26, ptr %27, i64 %28)
  %30 = icmp eq i32 %29, 0
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err9, label %err.ok10

err9:                                             ; preds = %err.ok8
  ret i1 false

err.ok10:                                         ; preds = %err.ok8
  ret i1 %30
}

define i1 @wsgi__prefix_matches({ ptr, i64 } %prefix, { ptr, i64 } %path) {
entry:
  %prefix1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %prefix, ptr %prefix1, align 8
  %path2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path2, align 8
  %0 = load { ptr, i64 }, ptr %path2, align 8
  %1 = load { ptr, i64 }, ptr %prefix1, align 8
  %2 = call i1 @string_starts_with({ ptr, i64 } %0, { ptr, i64 } %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

if.end:                                           ; preds = %if.next
  %5 = load { ptr, i64 }, ptr %path2, align 8
  %6 = extractvalue { ptr, i64 } %5, 1
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err6, label %err.ok7

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %9 = xor i1 %2, true
  br i1 %9, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err3, label %err.ok4

if.next:                                          ; preds = %err.ok
  br label %if.end

err3:                                             ; preds = %if.then
  ret i1 false

err.ok4:                                          ; preds = %if.then
  ret i1 false

if.end5:                                          ; preds = %if.next11
  %12 = load { ptr, i64 }, ptr %prefix1, align 8
  %13 = extractvalue { ptr, i64 } %12, 1
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err15, label %err.ok16

err6:                                             ; preds = %if.end
  ret i1 false

err.ok7:                                          ; preds = %if.end
  %16 = load { ptr, i64 }, ptr %prefix1, align 8
  %17 = extractvalue { ptr, i64 } %16, 1
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err8, label %err.ok9

err8:                                             ; preds = %err.ok7
  ret i1 false

err.ok9:                                          ; preds = %err.ok7
  %20 = icmp eq i64 %6, %17
  br i1 %20, label %if.then10, label %if.next11

if.then10:                                        ; preds = %err.ok9
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err12, label %err.ok13

if.next11:                                        ; preds = %err.ok9
  br label %if.end5

err12:                                            ; preds = %if.then10
  ret i1 false

err.ok13:                                         ; preds = %if.then10
  ret i1 true

if.end14:                                         ; preds = %if.next20
  %23 = load { ptr, i64 }, ptr %path2, align 8
  %si.data23 = alloca ptr, align 8
  %si.len24 = alloca i64, align 8
  %24 = extractvalue { ptr, i64 } %23, 0
  %25 = extractvalue { ptr, i64 } %23, 1
  %26 = load { ptr, i64 }, ptr %prefix1, align 8
  %27 = extractvalue { ptr, i64 } %26, 1
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err25, label %err.ok26

err15:                                            ; preds = %if.end5
  ret i1 false

err.ok16:                                         ; preds = %if.end5
  %30 = icmp sgt i64 %13, 0
  br i1 %30, label %log.rhs, label %log.end

log.rhs:                                          ; preds = %err.ok16
  %31 = load { ptr, i64 }, ptr %prefix1, align 8
  %si.data = alloca ptr, align 8
  %si.len = alloca i64, align 8
  %32 = extractvalue { ptr, i64 } %31, 0
  %33 = extractvalue { ptr, i64 } %31, 1
  %34 = load { ptr, i64 }, ptr %prefix1, align 8
  %35 = extractvalue { ptr, i64 } %34, 1
  %36 = call i32 @sere_has_error()
  %37 = icmp ne i32 %36, 0
  br i1 %37, label %err17, label %err.ok18

log.end:                                          ; preds = %err.ok18, %err.ok16
  %log.phi = phi i1 [ %30, %err.ok16 ], [ %46, %err.ok18 ]
  br i1 %log.phi, label %if.then19, label %if.next20

err17:                                            ; preds = %log.rhs
  ret i1 false

err.ok18:                                         ; preds = %log.rhs
  %38 = sub i64 %35, 1
  call void @sere_str_index(ptr %32, i64 %33, i64 %38, ptr %si.data, ptr %si.len)
  %39 = load i64, ptr %si.len, align 4
  %40 = load ptr, ptr %si.data, align 8
  %41 = insertvalue { ptr, i64 } undef, ptr %40, 0
  %42 = insertvalue { ptr, i64 } %41, i64 %39, 1
  %43 = extractvalue { ptr, i64 } %42, 0
  %44 = extractvalue { ptr, i64 } %42, 1
  %45 = call i32 @sere_str_cmp(ptr %43, i64 %44, ptr @94, i64 1)
  %46 = icmp eq i32 %45, 0
  br label %log.end

if.then19:                                        ; preds = %log.end
  %47 = call i32 @sere_has_error()
  %48 = icmp ne i32 %47, 0
  br i1 %48, label %err21, label %err.ok22

if.next20:                                        ; preds = %log.end
  br label %if.end14

err21:                                            ; preds = %if.then19
  ret i1 false

err.ok22:                                         ; preds = %if.then19
  ret i1 true

err25:                                            ; preds = %if.end14
  ret i1 false

err.ok26:                                         ; preds = %if.end14
  call void @sere_str_index(ptr %24, i64 %25, i64 %27, ptr %si.data23, ptr %si.len24)
  %49 = load i64, ptr %si.len24, align 4
  %50 = load ptr, ptr %si.data23, align 8
  %51 = insertvalue { ptr, i64 } undef, ptr %50, 0
  %52 = insertvalue { ptr, i64 } %51, i64 %49, 1
  %53 = extractvalue { ptr, i64 } %52, 0
  %54 = extractvalue { ptr, i64 } %52, 1
  %55 = call i32 @sere_str_cmp(ptr %53, i64 %54, ptr @95, i64 1)
  %56 = icmp eq i32 %55, 0
  %57 = call i32 @sere_has_error()
  %58 = icmp ne i32 %57, 0
  br i1 %58, label %err27, label %err.ok28

err27:                                            ; preds = %err.ok26
  ret i1 false

err.ok28:                                         ; preds = %err.ok26
  ret i1 %56
}

define i1 @wsgi_match_pattern({ ptr, i64 } %pattern, { ptr, i64 } %path, %MultiMap %captures) {
entry:
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %path2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path2, align 8
  %captures3 = alloca %MultiMap, align 8
  store %MultiMap %captures, ptr %captures3, align 8
  %want = alloca ptr, align 8
  %0 = load { ptr, i64 }, ptr %pattern1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call ptr @sere_string_split(ptr %1, i64 %2, ptr @96, i64 1)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  store ptr %3, ptr %want, align 8
  %have = alloca ptr, align 8
  %6 = load { ptr, i64 }, ptr %path2, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  %9 = call ptr @sere_string_split(ptr %7, i64 %8, ptr @97, i64 1)
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret i1 false

err.ok5:                                          ; preds = %err.ok
  store ptr %9, ptr %have, align 8
  %12 = load ptr, ptr %want, align 8
  %13 = call i64 @sere_list_len(ptr %12)
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err6, label %err.ok7

if.end:                                           ; preds = %if.next
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

err6:                                             ; preds = %err.ok5
  ret i1 false

err.ok7:                                          ; preds = %err.ok5
  %16 = load ptr, ptr %have, align 8
  %17 = call i64 @sere_list_len(ptr %16)
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err8, label %err.ok9

err8:                                             ; preds = %err.ok7
  ret i1 false

err.ok9:                                          ; preds = %err.ok7
  %20 = icmp ne i64 %13, %17
  br i1 %20, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok9
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err10, label %err.ok11

if.next:                                          ; preds = %err.ok9
  br label %if.end

err10:                                            ; preds = %if.then
  ret i1 false

err.ok11:                                         ; preds = %if.then
  ret i1 false

while.cond:                                       ; preds = %err.ok70, %if.end
  %23 = load i64, ptr %i, align 4
  %24 = load ptr, ptr %want, align 8
  %25 = call i64 @sere_list_len(ptr %24)
  %26 = call i32 @sere_has_error()
  %27 = icmp ne i32 %26, 0
  br i1 %27, label %err12, label %err.ok13

while.body:                                       ; preds = %err.ok13
  %segment = alloca { ptr, i64 }, align 8
  %28 = load ptr, ptr %want, align 8
  %29 = load i64, ptr %i, align 4
  %30 = call ptr @sere_list_item(ptr %28, i64 %29)
  %31 = load { ptr, i64 }, ptr %30, align 8
  store { ptr, i64 } %31, ptr %segment, align 8
  %32 = call i32 @sere_has_error()
  %33 = icmp ne i32 %32, 0
  br i1 %33, label %err14, label %err.ok15

while.end:                                        ; preds = %err.ok13
  %34 = call i32 @sere_has_error()
  %35 = icmp ne i32 %34, 0
  br i1 %35, label %err71, label %err.ok72

err12:                                            ; preds = %while.cond
  ret i1 false

err.ok13:                                         ; preds = %while.cond
  %36 = icmp slt i64 %23, %25
  br i1 %36, label %while.body, label %while.end

err14:                                            ; preds = %while.body
  ret i1 false

err.ok15:                                         ; preds = %while.body
  %37 = load { ptr, i64 }, ptr %segment, align 8
  %38 = extractvalue { ptr, i64 } %37, 1
  %39 = call i32 @sere_has_error()
  %40 = icmp ne i32 %39, 0
  br i1 %40, label %err17, label %err.ok18

if.end16:                                         ; preds = %if.next64, %err.ok62
  %41 = call i32 @sere_has_error()
  %42 = icmp ne i32 %41, 0
  br i1 %42, label %err67, label %err.ok68

err17:                                            ; preds = %err.ok15
  ret i1 false

err.ok18:                                         ; preds = %err.ok15
  %43 = icmp sgt i64 %38, 0
  br i1 %43, label %log.rhs, label %log.end

log.rhs:                                          ; preds = %err.ok18
  %44 = load { ptr, i64 }, ptr %segment, align 8
  %si.data = alloca ptr, align 8
  %si.len = alloca i64, align 8
  %45 = extractvalue { ptr, i64 } %44, 0
  %46 = extractvalue { ptr, i64 } %44, 1
  call void @sere_str_index(ptr %45, i64 %46, i64 0, ptr %si.data, ptr %si.len)
  %47 = load i64, ptr %si.len, align 4
  %48 = load ptr, ptr %si.data, align 8
  %49 = insertvalue { ptr, i64 } undef, ptr %48, 0
  %50 = insertvalue { ptr, i64 } %49, i64 %47, 1
  %51 = extractvalue { ptr, i64 } %50, 0
  %52 = extractvalue { ptr, i64 } %50, 1
  %53 = call i32 @sere_str_cmp(ptr %51, i64 %52, ptr @98, i64 1)
  %54 = icmp eq i32 %53, 0
  br i1 %54, label %log.end20, label %log.rhs19

log.end:                                          ; preds = %log.end20, %err.ok18
  %log.phi23 = phi i1 [ %43, %err.ok18 ], [ %log.phi, %log.end20 ]
  br i1 %log.phi23, label %if.then24, label %if.next25

log.rhs19:                                        ; preds = %log.rhs
  %55 = load { ptr, i64 }, ptr %segment, align 8
  %si.data21 = alloca ptr, align 8
  %si.len22 = alloca i64, align 8
  %56 = extractvalue { ptr, i64 } %55, 0
  %57 = extractvalue { ptr, i64 } %55, 1
  call void @sere_str_index(ptr %56, i64 %57, i64 0, ptr %si.data21, ptr %si.len22)
  %58 = load i64, ptr %si.len22, align 4
  %59 = load ptr, ptr %si.data21, align 8
  %60 = insertvalue { ptr, i64 } undef, ptr %59, 0
  %61 = insertvalue { ptr, i64 } %60, i64 %58, 1
  %62 = extractvalue { ptr, i64 } %61, 0
  %63 = extractvalue { ptr, i64 } %61, 1
  %64 = call i32 @sere_str_cmp(ptr %62, i64 %63, ptr @99, i64 1)
  %65 = icmp eq i32 %64, 0
  br label %log.end20

log.end20:                                        ; preds = %log.rhs19, %log.rhs
  %log.phi = phi i1 [ %54, %log.rhs ], [ %65, %log.rhs19 ]
  br label %log.end

if.then24:                                        ; preds = %log.end
  %name = alloca { ptr, i64 }, align 8
  %66 = load { ptr, i64 }, ptr %segment, align 8
  %sl.data = alloca ptr, align 8
  %sl.len = alloca i64, align 8
  %67 = extractvalue { ptr, i64 } %66, 0
  %68 = extractvalue { ptr, i64 } %66, 1
  call void @sere_str_slice(ptr %67, i64 %68, i64 1, i64 0, i32 1, i32 0, ptr %sl.data, ptr %sl.len)
  %69 = load i64, ptr %sl.len, align 4
  %70 = load ptr, ptr %sl.data, align 8
  %71 = insertvalue { ptr, i64 } undef, ptr %70, 0
  %72 = insertvalue { ptr, i64 } %71, i64 %69, 1
  store { ptr, i64 } %72, ptr %name, align 8
  %73 = call i32 @sere_has_error()
  %74 = icmp ne i32 %73, 0
  br i1 %74, label %err26, label %err.ok27

if.next25:                                        ; preds = %log.end
  %75 = load { ptr, i64 }, ptr %segment, align 8
  %76 = load ptr, ptr %have, align 8
  %77 = load i64, ptr %i, align 4
  %78 = call ptr @sere_list_item(ptr %76, i64 %77)
  %79 = load { ptr, i64 }, ptr %78, align 8
  %80 = extractvalue { ptr, i64 } %75, 0
  %81 = extractvalue { ptr, i64 } %75, 1
  %82 = extractvalue { ptr, i64 } %79, 0
  %83 = extractvalue { ptr, i64 } %79, 1
  %84 = call i32 @sere_str_cmp(ptr %80, i64 %81, ptr %82, i64 %83)
  %85 = icmp ne i32 %84, 0
  br i1 %85, label %if.then63, label %if.next64

err26:                                            ; preds = %if.then24
  ret i1 false

err.ok27:                                         ; preds = %if.then24
  %86 = load { ptr, i64 }, ptr %segment, align 8
  %87 = extractvalue { ptr, i64 } %86, 1
  %88 = call i32 @sere_has_error()
  %89 = icmp ne i32 %88, 0
  br i1 %89, label %err29, label %err.ok30

if.end28:                                         ; preds = %if.next39, %err.ok45
  %90 = call i32 @sere_has_error()
  %91 = icmp ne i32 %90, 0
  br i1 %91, label %err46, label %err.ok47

err29:                                            ; preds = %err.ok27
  ret i1 false

err.ok30:                                         ; preds = %err.ok27
  %92 = icmp sge i64 %87, 2
  br i1 %92, label %log.rhs31, label %log.end32

log.rhs31:                                        ; preds = %err.ok30
  %93 = load { ptr, i64 }, ptr %segment, align 8
  %si.data33 = alloca ptr, align 8
  %si.len34 = alloca i64, align 8
  %94 = extractvalue { ptr, i64 } %93, 0
  %95 = extractvalue { ptr, i64 } %93, 1
  %96 = load { ptr, i64 }, ptr %segment, align 8
  %97 = extractvalue { ptr, i64 } %96, 1
  %98 = call i32 @sere_has_error()
  %99 = icmp ne i32 %98, 0
  br i1 %99, label %err35, label %err.ok36

log.end32:                                        ; preds = %err.ok36, %err.ok30
  %log.phi37 = phi i1 [ %92, %err.ok30 ], [ %108, %err.ok36 ]
  br i1 %log.phi37, label %if.then38, label %if.next39

err35:                                            ; preds = %log.rhs31
  ret i1 false

err.ok36:                                         ; preds = %log.rhs31
  %100 = sub i64 %97, 1
  call void @sere_str_index(ptr %94, i64 %95, i64 %100, ptr %si.data33, ptr %si.len34)
  %101 = load i64, ptr %si.len34, align 4
  %102 = load ptr, ptr %si.data33, align 8
  %103 = insertvalue { ptr, i64 } undef, ptr %102, 0
  %104 = insertvalue { ptr, i64 } %103, i64 %101, 1
  %105 = extractvalue { ptr, i64 } %104, 0
  %106 = extractvalue { ptr, i64 } %104, 1
  %107 = call i32 @sere_str_cmp(ptr %105, i64 %106, ptr @100, i64 1)
  %108 = icmp eq i32 %107, 0
  br label %log.end32

if.then38:                                        ; preds = %log.end32
  %109 = load { ptr, i64 }, ptr %segment, align 8
  %sl.data40 = alloca ptr, align 8
  %sl.len41 = alloca i64, align 8
  %110 = load { ptr, i64 }, ptr %segment, align 8
  %111 = extractvalue { ptr, i64 } %110, 1
  %112 = call i32 @sere_has_error()
  %113 = icmp ne i32 %112, 0
  br i1 %113, label %err42, label %err.ok43

if.next39:                                        ; preds = %log.end32
  br label %if.end28

err42:                                            ; preds = %if.then38
  ret i1 false

err.ok43:                                         ; preds = %if.then38
  %114 = sub i64 %111, 1
  %115 = extractvalue { ptr, i64 } %109, 0
  %116 = extractvalue { ptr, i64 } %109, 1
  call void @sere_str_slice(ptr %115, i64 %116, i64 1, i64 %114, i32 1, i32 1, ptr %sl.data40, ptr %sl.len41)
  %117 = load i64, ptr %sl.len41, align 4
  %118 = load ptr, ptr %sl.data40, align 8
  %119 = insertvalue { ptr, i64 } undef, ptr %118, 0
  %120 = insertvalue { ptr, i64 } %119, i64 %117, 1
  store { ptr, i64 } %120, ptr %name, align 8
  %121 = call i32 @sere_has_error()
  %122 = icmp ne i32 %121, 0
  br i1 %122, label %err44, label %err.ok45

err44:                                            ; preds = %err.ok43
  ret i1 false

err.ok45:                                         ; preds = %err.ok43
  br label %if.end28

err46:                                            ; preds = %if.end28
  ret i1 false

err.ok47:                                         ; preds = %if.end28
  %123 = load { ptr, i64 }, ptr %name, align 8
  %124 = extractvalue { ptr, i64 } %123, 1
  %125 = call i32 @sere_has_error()
  %126 = icmp ne i32 %125, 0
  br i1 %126, label %err49, label %err.ok50

if.end48:                                         ; preds = %if.next52
  %127 = call i32 @sere_has_error()
  %128 = icmp ne i32 %127, 0
  br i1 %128, label %err55, label %err.ok56

err49:                                            ; preds = %err.ok47
  ret i1 false

err.ok50:                                         ; preds = %err.ok47
  %129 = icmp eq i64 %124, 0
  br i1 %129, label %if.then51, label %if.next52

if.then51:                                        ; preds = %err.ok50
  %130 = call i32 @sere_has_error()
  %131 = icmp ne i32 %130, 0
  br i1 %131, label %err53, label %err.ok54

if.next52:                                        ; preds = %err.ok50
  br label %if.end48

err53:                                            ; preds = %if.then51
  ret i1 false

err.ok54:                                         ; preds = %if.then51
  ret i1 false

err55:                                            ; preds = %if.end48
  ret i1 false

err.ok56:                                         ; preds = %if.end48
  %132 = load { ptr, i64 }, ptr %name, align 8
  %133 = load ptr, ptr %have, align 8
  %134 = load i64, ptr %i, align 4
  %135 = call ptr @sere_list_item(ptr %133, i64 %134)
  %136 = load { ptr, i64 }, ptr %135, align 8
  %137 = extractvalue { ptr, i64 } %136, 0
  %138 = extractvalue { ptr, i64 } %136, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_url_decode(ptr %137, i64 %138, ptr %ext.str.data, ptr %ext.str.len)
  %139 = load i64, ptr %ext.str.len, align 4
  %140 = load ptr, ptr %ext.str.data, align 8
  %141 = insertvalue { ptr, i64 } undef, ptr %140, 0
  %142 = insertvalue { ptr, i64 } %141, i64 %139, 1
  %143 = call i32 @sere_has_error()
  %144 = icmp ne i32 %143, 0
  br i1 %144, label %err57, label %err.ok58

err57:                                            ; preds = %err.ok56
  ret i1 false

err.ok58:                                         ; preds = %err.ok56
  call void @wsgi_MultiMap_add(ptr %captures3, { ptr, i64 } %132, { ptr, i64 } %142)
  %145 = call i32 @sere_has_error()
  %146 = icmp ne i32 %145, 0
  br i1 %146, label %err59, label %err.ok60

err59:                                            ; preds = %err.ok58
  ret i1 false

err.ok60:                                         ; preds = %err.ok58
  %147 = call i32 @sere_has_error()
  %148 = icmp ne i32 %147, 0
  br i1 %148, label %err61, label %err.ok62

err61:                                            ; preds = %err.ok60
  ret i1 false

err.ok62:                                         ; preds = %err.ok60
  br label %if.end16

if.then63:                                        ; preds = %if.next25
  %149 = call i32 @sere_has_error()
  %150 = icmp ne i32 %149, 0
  br i1 %150, label %err65, label %err.ok66

if.next64:                                        ; preds = %if.next25
  br label %if.end16

err65:                                            ; preds = %if.then63
  ret i1 false

err.ok66:                                         ; preds = %if.then63
  ret i1 false

err67:                                            ; preds = %if.end16
  ret i1 false

err.ok68:                                         ; preds = %if.end16
  %151 = load i64, ptr %i, align 4
  %152 = add i64 %151, 1
  store i64 %152, ptr %i, align 4
  %153 = call i32 @sere_has_error()
  %154 = icmp ne i32 %153, 0
  br i1 %154, label %err69, label %err.ok70

err69:                                            ; preds = %err.ok68
  ret i1 false

err.ok70:                                         ; preds = %err.ok68
  br label %while.cond

err71:                                            ; preds = %while.end
  ret i1 false

err.ok72:                                         ; preds = %while.end
  ret i1 true
}

define void @wsgi_Router___init__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Router, ptr %0, i32 0, i32 1
  %2 = call ptr @sere_list_new(i64 56)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store ptr %2, ptr %1, align 8
  ret void
}

define void @wsgi_Router_add(ptr %self, { ptr, i64 } %method, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %method1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %method, ptr %method1, align 8
  %pattern2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern2, align 8
  %handler3 = alloca ptr, align 8
  store ptr %handler, ptr %handler3, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Router, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Router, ptr %3, i32 0, i32 1
  %5 = load ptr, ptr %4, align 8
  %init.tmp = alloca %Route, align 8
  store %Route zeroinitializer, ptr %init.tmp, align 8
  %6 = getelementptr inbounds nuw %Route, ptr %init.tmp, i32 0, i32 0
  store i32 -673828674, ptr %6, align 4
  %7 = load { ptr, i64 }, ptr %method1, align 8
  %8 = load { ptr, i64 }, ptr %pattern2, align 8
  %9 = load ptr, ptr %handler3, align 8
  call void @wsgi_Route___init__(ptr %init.tmp, { ptr, i64 } %7, { ptr, i64 } %8, i32 2, ptr %9)
  %10 = load %Route, ptr %init.tmp, align 8
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  %tmp.slot = alloca %Route, align 8
  store %Route %10, ptr %tmp.slot, align 8
  call void @sere_list_push(ptr %5, ptr %tmp.slot)
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret void

err.ok5:                                          ; preds = %err.ok
  ret void
}

define void @wsgi_Router_any(ptr %self, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %pattern1, align 8
  %2 = load ptr, ptr %handler2, align 8
  call void @wsgi_Router_add(ptr %0, { ptr, i64 } { ptr @101, i64 1 }, { ptr, i64 } %1, ptr %2)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Router_get(ptr %self, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %pattern1, align 8
  %2 = load ptr, ptr %handler2, align 8
  call void @wsgi_Router_add(ptr %0, { ptr, i64 } { ptr @102, i64 3 }, { ptr, i64 } %1, ptr %2)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Router_post(ptr %self, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %pattern1, align 8
  %2 = load ptr, ptr %handler2, align 8
  call void @wsgi_Router_add(ptr %0, { ptr, i64 } { ptr @103, i64 4 }, { ptr, i64 } %1, ptr %2)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Router_put(ptr %self, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %pattern1, align 8
  %2 = load ptr, ptr %handler2, align 8
  call void @wsgi_Router_add(ptr %0, { ptr, i64 } { ptr @104, i64 3 }, { ptr, i64 } %1, ptr %2)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Router_patch(ptr %self, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %pattern1, align 8
  %2 = load ptr, ptr %handler2, align 8
  call void @wsgi_Router_add(ptr %0, { ptr, i64 } { ptr @105, i64 5 }, { ptr, i64 } %1, ptr %2)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Router_delete(ptr %self, { ptr, i64 } %pattern, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %pattern1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %pattern, ptr %pattern1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load { ptr, i64 }, ptr %pattern1, align 8
  %2 = load ptr, ptr %handler2, align 8
  call void @wsgi_Router_add(ptr %0, { ptr, i64 } { ptr @106, i64 6 }, { ptr, i64 } %1, ptr %2)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define void @wsgi_Router_mount(ptr %self, { ptr, i64 } %prefix, ptr %handler) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %prefix1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %prefix, ptr %prefix1, align 8
  %handler2 = alloca ptr, align 8
  store ptr %handler, ptr %handler2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Router, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Router, ptr %3, i32 0, i32 1
  %5 = load ptr, ptr %4, align 8
  %init.tmp = alloca %Route, align 8
  store %Route zeroinitializer, ptr %init.tmp, align 8
  %6 = getelementptr inbounds nuw %Route, ptr %init.tmp, i32 0, i32 0
  store i32 -673828674, ptr %6, align 4
  %7 = load { ptr, i64 }, ptr %prefix1, align 8
  %8 = load ptr, ptr %handler2, align 8
  call void @wsgi_Route___init__(ptr %init.tmp, { ptr, i64 } { ptr @107, i64 1 }, { ptr, i64 } %7, i32 1, ptr %8)
  %9 = load %Route, ptr %init.tmp, align 8
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  %tmp.slot = alloca %Route, align 8
  store %Route %9, ptr %tmp.slot, align 8
  call void @sere_list_push(ptr %5, ptr %tmp.slot)
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret void

err.ok4:                                          ; preds = %err.ok
  ret void
}

define i64 @wsgi_Router_count(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Router, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = call i64 @sere_list_len(ptr %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i64 0

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i64 0

err.ok2:                                          ; preds = %err.ok
  ret i64 %3
}

define %Response @wsgi_Router_dispatch(ptr %self, %Environ %environ) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %environ1 = alloca %Environ, align 8
  store %Environ %environ, ptr %environ1, align 8
  %i = alloca i64, align 8
  store i64 0, ptr %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %err.ok45, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load ptr, ptr %self.slot, align 8
  %2 = getelementptr inbounds nuw %Router, ptr %1, i32 0, i32 1
  %3 = load ptr, ptr %2, align 8
  %4 = call i64 @sere_list_len(ptr %3)
  %5 = call i32 @sere_has_error()
  %6 = icmp ne i32 %5, 0
  br i1 %6, label %err, label %err.ok

while.body:                                       ; preds = %err.ok
  %route = alloca %Route, align 8
  %7 = load ptr, ptr %self.slot, align 8
  %8 = getelementptr inbounds nuw %Router, ptr %7, i32 0, i32 1
  %9 = load ptr, ptr %8, align 8
  %10 = load i64, ptr %i, align 4
  %11 = call ptr @sere_list_item(ptr %9, i64 %10)
  %12 = load %Route, ptr %11, align 8
  store %Route %12, ptr %route, align 8
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err2, label %err.ok3

while.end:                                        ; preds = %err.ok
  %15 = call %Response @wsgi_not_found({ ptr, i64 } { ptr @108, i64 9 })
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err46, label %err.ok47

err:                                              ; preds = %while.cond
  ret %Response zeroinitializer

err.ok:                                           ; preds = %while.cond
  %18 = icmp slt i64 %0, %4
  br i1 %18, label %while.body, label %while.end

err2:                                             ; preds = %while.body
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %while.body
  %19 = getelementptr inbounds nuw %Route, ptr %route, i32 0, i32 1
  %20 = load { ptr, i64 }, ptr %19, align 8
  %21 = getelementptr inbounds nuw %Environ, ptr %environ1, i32 0, i32 1
  %22 = load { ptr, i64 }, ptr %21, align 8
  %23 = call i1 @wsgi__method_matches({ ptr, i64 } %20, { ptr, i64 } %22)
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err4, label %err.ok5

if.end:                                           ; preds = %if.next, %err.ok41
  %26 = call i32 @sere_has_error()
  %27 = icmp ne i32 %26, 0
  br i1 %27, label %err42, label %err.ok43

err4:                                             ; preds = %err.ok3
  ret %Response zeroinitializer

err.ok5:                                          ; preds = %err.ok3
  br i1 %23, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok5
  %28 = getelementptr inbounds nuw %Route, ptr %route, i32 0, i32 3
  %29 = load i32, ptr %28, align 4
  %30 = icmp eq i32 %29, 1
  br i1 %30, label %if.then7, label %if.next8

if.next:                                          ; preds = %err.ok5
  br label %if.end

if.end6:                                          ; preds = %err.ok39, %err.ok19
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err40, label %err.ok41

if.then7:                                         ; preds = %if.then
  %33 = getelementptr inbounds nuw %Route, ptr %route, i32 0, i32 2
  %34 = load { ptr, i64 }, ptr %33, align 8
  %35 = getelementptr inbounds nuw %Environ, ptr %environ1, i32 0, i32 2
  %36 = load { ptr, i64 }, ptr %35, align 8
  %37 = call i1 @wsgi__prefix_matches({ ptr, i64 } %34, { ptr, i64 } %36)
  %38 = call i32 @sere_has_error()
  %39 = icmp ne i32 %38, 0
  br i1 %39, label %err10, label %err.ok11

if.next8:                                         ; preds = %if.then
  %40 = getelementptr inbounds nuw %Route, ptr %route, i32 0, i32 2
  %41 = load { ptr, i64 }, ptr %40, align 8
  %42 = getelementptr inbounds nuw %Environ, ptr %environ1, i32 0, i32 2
  %43 = load { ptr, i64 }, ptr %42, align 8
  %44 = getelementptr inbounds nuw %Environ, ptr %environ1, i32 0, i32 7
  %45 = load %MultiMap, ptr %44, align 8
  %46 = call i1 @wsgi_match_pattern({ ptr, i64 } %41, { ptr, i64 } %43, %MultiMap %45)
  %47 = call i32 @sere_has_error()
  %48 = icmp ne i32 %47, 0
  br i1 %48, label %err21, label %err.ok22

if.end9:                                          ; preds = %if.next13
  %49 = call i32 @sere_has_error()
  %50 = icmp ne i32 %49, 0
  br i1 %50, label %err18, label %err.ok19

err10:                                            ; preds = %if.then7
  ret %Response zeroinitializer

err.ok11:                                         ; preds = %if.then7
  br i1 %37, label %if.then12, label %if.next13

if.then12:                                        ; preds = %err.ok11
  %51 = getelementptr inbounds nuw %Route, ptr %route, i32 0, i32 4
  %52 = load ptr, ptr %51, align 8
  %53 = load %Environ, ptr %environ1, align 8
  %cb.fat = load { ptr, ptr }, ptr %52, align 8
  %cb.fn = extractvalue { ptr, ptr } %cb.fat, 0
  %cb.env = extractvalue { ptr, ptr } %cb.fat, 1
  %54 = icmp ne ptr %cb.env, null
  br i1 %54, label %cb.bound, label %cb.free

if.next13:                                        ; preds = %err.ok11
  br label %if.end9

cb.bound:                                         ; preds = %if.then12
  %cb.bound.ret = call %Response %cb.fn(ptr %cb.env, %Environ %53)
  br label %cb.merge

cb.free:                                          ; preds = %if.then12
  %cb.free.ret = call %Response %cb.fn(%Environ %53)
  br label %cb.merge

cb.merge:                                         ; preds = %cb.free, %cb.bound
  %cb.ret = phi %Response [ %cb.bound.ret, %cb.bound ], [ %cb.free.ret, %cb.free ]
  %55 = call i32 @sere_has_error()
  %56 = icmp ne i32 %55, 0
  br i1 %56, label %err14, label %err.ok15

err14:                                            ; preds = %cb.merge
  ret %Response zeroinitializer

err.ok15:                                         ; preds = %cb.merge
  %57 = call i32 @sere_has_error()
  %58 = icmp ne i32 %57, 0
  br i1 %58, label %err16, label %err.ok17

err16:                                            ; preds = %err.ok15
  ret %Response zeroinitializer

err.ok17:                                         ; preds = %err.ok15
  ret %Response %cb.ret

err18:                                            ; preds = %if.end9
  ret %Response zeroinitializer

err.ok19:                                         ; preds = %if.end9
  br label %if.end6

if.end20:                                         ; preds = %if.next24
  %59 = call i32 @sere_has_error()
  %60 = icmp ne i32 %59, 0
  br i1 %60, label %err38, label %err.ok39

err21:                                            ; preds = %if.next8
  ret %Response zeroinitializer

err.ok22:                                         ; preds = %if.next8
  br i1 %46, label %if.then23, label %if.next24

if.then23:                                        ; preds = %err.ok22
  %61 = getelementptr inbounds nuw %Route, ptr %route, i32 0, i32 4
  %62 = load ptr, ptr %61, align 8
  %63 = load %Environ, ptr %environ1, align 8
  %cb.fat25 = load { ptr, ptr }, ptr %62, align 8
  %cb.fn26 = extractvalue { ptr, ptr } %cb.fat25, 0
  %cb.env27 = extractvalue { ptr, ptr } %cb.fat25, 1
  %64 = icmp ne ptr %cb.env27, null
  br i1 %64, label %cb.bound28, label %cb.free29

if.next24:                                        ; preds = %err.ok22
  br label %if.end20

cb.bound28:                                       ; preds = %if.then23
  %cb.bound.ret31 = call %Response %cb.fn26(ptr %cb.env27, %Environ %63)
  br label %cb.merge30

cb.free29:                                        ; preds = %if.then23
  %cb.free.ret32 = call %Response %cb.fn26(%Environ %63)
  br label %cb.merge30

cb.merge30:                                       ; preds = %cb.free29, %cb.bound28
  %cb.ret33 = phi %Response [ %cb.bound.ret31, %cb.bound28 ], [ %cb.free.ret32, %cb.free29 ]
  %65 = call i32 @sere_has_error()
  %66 = icmp ne i32 %65, 0
  br i1 %66, label %err34, label %err.ok35

err34:                                            ; preds = %cb.merge30
  ret %Response zeroinitializer

err.ok35:                                         ; preds = %cb.merge30
  %67 = call i32 @sere_has_error()
  %68 = icmp ne i32 %67, 0
  br i1 %68, label %err36, label %err.ok37

err36:                                            ; preds = %err.ok35
  ret %Response zeroinitializer

err.ok37:                                         ; preds = %err.ok35
  ret %Response %cb.ret33

err38:                                            ; preds = %if.end20
  ret %Response zeroinitializer

err.ok39:                                         ; preds = %if.end20
  br label %if.end6

err40:                                            ; preds = %if.end6
  ret %Response zeroinitializer

err.ok41:                                         ; preds = %if.end6
  br label %if.end

err42:                                            ; preds = %if.end
  ret %Response zeroinitializer

err.ok43:                                         ; preds = %if.end
  %69 = load i64, ptr %i, align 4
  %70 = add i64 %69, 1
  store i64 %70, ptr %i, align 4
  %71 = call i32 @sere_has_error()
  %72 = icmp ne i32 %71, 0
  br i1 %72, label %err44, label %err.ok45

err44:                                            ; preds = %err.ok43
  ret %Response zeroinitializer

err.ok45:                                         ; preds = %err.ok43
  br label %while.cond

err46:                                            ; preds = %while.end
  ret %Response zeroinitializer

err.ok47:                                         ; preds = %while.end
  %73 = call i32 @sere_has_error()
  %74 = icmp ne i32 %73, 0
  br i1 %74, label %err48, label %err.ok49

err48:                                            ; preds = %err.ok47
  ret %Response zeroinitializer

err.ok49:                                         ; preds = %err.ok47
  ret %Response %15
}

define void @wsgi_Files___init__(ptr %self, { ptr, i64 } %root, { ptr, i64 } %prefix) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %root1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %root, ptr %root1, align 8
  %prefix2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %prefix, ptr %prefix2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Files, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %root1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Files, ptr %3, i32 0, i32 2
  %5 = load { ptr, i64 }, ptr %prefix2, align 8
  store { ptr, i64 } %5, ptr %4, align 8
  ret void
}

define %Response @wsgi_Files_handle(ptr %self, %Environ %environ) {
entry:
  %error = alloca %Exception, align 8
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %environ1 = alloca %Environ, align 8
  store %Environ %environ, ptr %environ1, align 8
  %rel = alloca { ptr, i64 }, align 8
  %0 = getelementptr inbounds nuw %Environ, ptr %environ1, i32 0, i32 2
  %1 = load { ptr, i64 }, ptr %0, align 8
  store { ptr, i64 } %1, ptr %rel, align 8
  %2 = load ptr, ptr %self.slot, align 8
  %3 = getelementptr inbounds nuw %Files, ptr %2, i32 0, i32 2
  %4 = load { ptr, i64 }, ptr %3, align 8
  %5 = extractvalue { ptr, i64 } %4, 1
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err, label %err.ok

if.end:                                           ; preds = %if.next, %err.ok7
  %8 = load { ptr, i64 }, ptr %rel, align 8
  %9 = extractvalue { ptr, i64 } %8, 1
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err9, label %err.ok10

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %12 = icmp sgt i64 %5, 0
  br i1 %12, label %log.rhs, label %log.end

log.rhs:                                          ; preds = %err.ok
  %13 = load { ptr, i64 }, ptr %rel, align 8
  %14 = load ptr, ptr %self.slot, align 8
  %15 = getelementptr inbounds nuw %Files, ptr %14, i32 0, i32 2
  %16 = load { ptr, i64 }, ptr %15, align 8
  %17 = call i1 @string_starts_with({ ptr, i64 } %13, { ptr, i64 } %16)
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err2, label %err.ok3

log.end:                                          ; preds = %err.ok3, %err.ok
  %log.phi = phi i1 [ %12, %err.ok ], [ %17, %err.ok3 ]
  br i1 %log.phi, label %if.then, label %if.next

err2:                                             ; preds = %log.rhs
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %log.rhs
  br label %log.end

if.then:                                          ; preds = %log.end
  %20 = load { ptr, i64 }, ptr %rel, align 8
  %sl.data = alloca ptr, align 8
  %sl.len = alloca i64, align 8
  %21 = load ptr, ptr %self.slot, align 8
  %22 = getelementptr inbounds nuw %Files, ptr %21, i32 0, i32 2
  %23 = load { ptr, i64 }, ptr %22, align 8
  %24 = extractvalue { ptr, i64 } %23, 1
  %25 = call i32 @sere_has_error()
  %26 = icmp ne i32 %25, 0
  br i1 %26, label %err4, label %err.ok5

if.next:                                          ; preds = %log.end
  br label %if.end

err4:                                             ; preds = %if.then
  ret %Response zeroinitializer

err.ok5:                                          ; preds = %if.then
  %27 = extractvalue { ptr, i64 } %20, 0
  %28 = extractvalue { ptr, i64 } %20, 1
  call void @sere_str_slice(ptr %27, i64 %28, i64 %24, i64 0, i32 1, i32 0, ptr %sl.data, ptr %sl.len)
  %29 = load i64, ptr %sl.len, align 4
  %30 = load ptr, ptr %sl.data, align 8
  %31 = insertvalue { ptr, i64 } undef, ptr %30, 0
  %32 = insertvalue { ptr, i64 } %31, i64 %29, 1
  store { ptr, i64 } %32, ptr %rel, align 8
  %33 = call i32 @sere_has_error()
  %34 = icmp ne i32 %33, 0
  br i1 %34, label %err6, label %err.ok7

err6:                                             ; preds = %err.ok5
  ret %Response zeroinitializer

err.ok7:                                          ; preds = %err.ok5
  br label %if.end

if.end8:                                          ; preds = %if.next17, %err.ok19
  %35 = load { ptr, i64 }, ptr %rel, align 8
  %36 = call i1 @string_contains({ ptr, i64 } %35, { ptr, i64 } { ptr @111, i64 2 })
  %37 = call i32 @sere_has_error()
  %38 = icmp ne i32 %37, 0
  br i1 %38, label %err21, label %err.ok22

err9:                                             ; preds = %if.end
  ret %Response zeroinitializer

err.ok10:                                         ; preds = %if.end
  %39 = icmp eq i64 %9, 0
  br i1 %39, label %log.end12, label %log.rhs11

log.rhs11:                                        ; preds = %err.ok10
  %40 = load { ptr, i64 }, ptr %rel, align 8
  %si.data = alloca ptr, align 8
  %si.len = alloca i64, align 8
  %41 = extractvalue { ptr, i64 } %40, 0
  %42 = extractvalue { ptr, i64 } %40, 1
  %43 = load { ptr, i64 }, ptr %rel, align 8
  %44 = extractvalue { ptr, i64 } %43, 1
  %45 = call i32 @sere_has_error()
  %46 = icmp ne i32 %45, 0
  br i1 %46, label %err13, label %err.ok14

log.end12:                                        ; preds = %err.ok14, %err.ok10
  %log.phi15 = phi i1 [ %39, %err.ok10 ], [ %55, %err.ok14 ]
  br i1 %log.phi15, label %if.then16, label %if.next17

err13:                                            ; preds = %log.rhs11
  ret %Response zeroinitializer

err.ok14:                                         ; preds = %log.rhs11
  %47 = sub i64 %44, 1
  call void @sere_str_index(ptr %41, i64 %42, i64 %47, ptr %si.data, ptr %si.len)
  %48 = load i64, ptr %si.len, align 4
  %49 = load ptr, ptr %si.data, align 8
  %50 = insertvalue { ptr, i64 } undef, ptr %49, 0
  %51 = insertvalue { ptr, i64 } %50, i64 %48, 1
  %52 = extractvalue { ptr, i64 } %51, 0
  %53 = extractvalue { ptr, i64 } %51, 1
  %54 = call i32 @sere_str_cmp(ptr %52, i64 %53, ptr @109, i64 1)
  %55 = icmp eq i32 %54, 0
  br label %log.end12

if.then16:                                        ; preds = %log.end12
  %56 = load { ptr, i64 }, ptr %rel, align 8
  %cat.len = alloca i64, align 8
  %57 = extractvalue { ptr, i64 } %56, 0
  %58 = extractvalue { ptr, i64 } %56, 1
  %59 = call ptr @sere_str_concat_data(ptr %57, i64 %58, ptr @110, i64 10, ptr %cat.len)
  %60 = load i64, ptr %cat.len, align 4
  %61 = insertvalue { ptr, i64 } undef, ptr %59, 0
  %62 = insertvalue { ptr, i64 } %61, i64 %60, 1
  store { ptr, i64 } %62, ptr %rel, align 8
  %63 = call i32 @sere_has_error()
  %64 = icmp ne i32 %63, 0
  br i1 %64, label %err18, label %err.ok19

if.next17:                                        ; preds = %log.end12
  br label %if.end8

err18:                                            ; preds = %if.then16
  ret %Response zeroinitializer

err.ok19:                                         ; preds = %if.then16
  br label %if.end8

if.end20:                                         ; preds = %if.next24
  %full = alloca { ptr, i64 }, align 8
  %65 = load ptr, ptr %self.slot, align 8
  %66 = getelementptr inbounds nuw %Files, ptr %65, i32 0, i32 1
  %67 = load { ptr, i64 }, ptr %66, align 8
  %68 = load { ptr, i64 }, ptr %rel, align 8
  %cat.len29 = alloca i64, align 8
  %69 = extractvalue { ptr, i64 } %67, 0
  %70 = extractvalue { ptr, i64 } %67, 1
  %71 = extractvalue { ptr, i64 } %68, 0
  %72 = extractvalue { ptr, i64 } %68, 1
  %73 = call ptr @sere_str_concat_data(ptr %69, i64 %70, ptr %71, i64 %72, ptr %cat.len29)
  %74 = load i64, ptr %cat.len29, align 4
  %75 = insertvalue { ptr, i64 } undef, ptr %73, 0
  %76 = insertvalue { ptr, i64 } %75, i64 %74, 1
  store { ptr, i64 } %76, ptr %full, align 8
  %77 = load { ptr, i64 }, ptr %full, align 8
  %78 = call i1 @fs_exists({ ptr, i64 } %77)
  %79 = call i32 @sere_has_error()
  %80 = icmp ne i32 %79, 0
  br i1 %80, label %err31, label %err.ok32

err21:                                            ; preds = %if.end8
  ret %Response zeroinitializer

err.ok22:                                         ; preds = %if.end8
  br i1 %36, label %if.then23, label %if.next24

if.then23:                                        ; preds = %err.ok22
  %81 = call %Response @wsgi_forbidden({ ptr, i64 } { ptr @112, i64 9 })
  %82 = call i32 @sere_has_error()
  %83 = icmp ne i32 %82, 0
  br i1 %83, label %err25, label %err.ok26

if.next24:                                        ; preds = %err.ok22
  br label %if.end20

err25:                                            ; preds = %if.then23
  ret %Response zeroinitializer

err.ok26:                                         ; preds = %if.then23
  %84 = call i32 @sere_has_error()
  %85 = icmp ne i32 %84, 0
  br i1 %85, label %err27, label %err.ok28

err27:                                            ; preds = %err.ok26
  ret %Response zeroinitializer

err.ok28:                                         ; preds = %err.ok26
  ret %Response %81

if.end30:                                         ; preds = %if.next34
  %body = alloca { ptr, i64 }, align 8
  %86 = load { ptr, i64 }, ptr %full, align 8
  %87 = extractvalue { ptr, i64 } %86, 0
  %88 = extractvalue { ptr, i64 } %86, 1
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_fs_read_text(ptr %87, i64 %88, ptr %ext.str.data, ptr %ext.str.len)
  %89 = load i64, ptr %ext.str.len, align 4
  %90 = load ptr, ptr %ext.str.data, align 8
  %91 = insertvalue { ptr, i64 } undef, ptr %90, 0
  %92 = insertvalue { ptr, i64 } %91, i64 %89, 1
  %93 = call i32 @sere_has_error()
  %94 = icmp ne i32 %93, 0
  br i1 %94, label %err39, label %err.ok40

err31:                                            ; preds = %if.end20
  ret %Response zeroinitializer

err.ok32:                                         ; preds = %if.end20
  %95 = xor i1 %78, true
  br i1 %95, label %if.then33, label %if.next34

if.then33:                                        ; preds = %err.ok32
  %96 = call %Response @wsgi_not_found({ ptr, i64 } { ptr @113, i64 9 })
  %97 = call i32 @sere_has_error()
  %98 = icmp ne i32 %97, 0
  br i1 %98, label %err35, label %err.ok36

if.next34:                                        ; preds = %err.ok32
  br label %if.end30

err35:                                            ; preds = %if.then33
  ret %Response zeroinitializer

err.ok36:                                         ; preds = %if.then33
  %99 = call i32 @sere_has_error()
  %100 = icmp ne i32 %99, 0
  br i1 %100, label %err37, label %err.ok38

err37:                                            ; preds = %err.ok36
  ret %Response zeroinitializer

err.ok38:                                         ; preds = %err.ok36
  ret %Response %96

try.dispatch:                                     ; preds = %err47, %err45, %err43, %err41, %err39
  %101 = call i32 @sere_error_isa(ptr @114)
  %102 = icmp ne i32 %101, 0
  br i1 %102, label %try.except, label %try.next

try.finally:                                      ; preds = %try.next, %except.error
  call void @sere_error_enter()
  call void @sere_error_leave(i32 1)
  br label %try.after

try.after:                                        ; preds = %finally.error, %try.finally
  %103 = call i32 @sere_has_error()
  %104 = icmp ne i32 %103, 0
  br i1 %104, label %err54, label %err.ok55

err39:                                            ; preds = %if.end30
  br label %try.dispatch

err.ok40:                                         ; preds = %if.end30
  store { ptr, i64 } %92, ptr %body, align 8
  %105 = call i32 @sere_has_error()
  %106 = icmp ne i32 %105, 0
  br i1 %106, label %err41, label %err.ok42

err41:                                            ; preds = %err.ok40
  br label %try.dispatch

err.ok42:                                         ; preds = %err.ok40
  %init.tmp = alloca %Response, align 8
  store %Response zeroinitializer, ptr %init.tmp, align 8
  %107 = getelementptr inbounds nuw %Response, ptr %init.tmp, i32 0, i32 0
  store i32 1437172478, ptr %107, align 4
  %108 = load { ptr, i64 }, ptr %rel, align 8
  %109 = call { ptr, i64 } @wsgi_content_type_for({ ptr, i64 } %108)
  %110 = call i32 @sere_has_error()
  %111 = icmp ne i32 %110, 0
  br i1 %111, label %err43, label %err.ok44

err43:                                            ; preds = %err.ok42
  br label %try.dispatch

err.ok44:                                         ; preds = %err.ok42
  %112 = load { ptr, i64 }, ptr %body, align 8
  call void @wsgi_Response___init__(ptr %init.tmp, i32 200, { ptr, i64 } %109, { ptr, i64 } %112)
  %113 = load %Response, ptr %init.tmp, align 8
  %114 = call i32 @sere_has_error()
  %115 = icmp ne i32 %114, 0
  br i1 %115, label %err45, label %err.ok46

err45:                                            ; preds = %err.ok44
  br label %try.dispatch

err.ok46:                                         ; preds = %err.ok44
  %116 = call i32 @sere_has_error()
  %117 = icmp ne i32 %116, 0
  br i1 %117, label %err47, label %err.ok48

err47:                                            ; preds = %err.ok46
  br label %try.dispatch

err.ok48:                                         ; preds = %err.ok46
  ret %Response %113

try.except:                                       ; preds = %try.dispatch
  %118 = alloca i64, align 8
  %119 = call ptr @sere_error_message(ptr %118)
  %120 = load i64, ptr %118, align 4
  %121 = insertvalue { ptr, i64 } undef, ptr %119, 0
  %122 = insertvalue { ptr, i64 } %121, i64 %120, 1
  %cat.len49 = alloca i64, align 8
  %123 = extractvalue { ptr, i64 } %122, 0
  %124 = extractvalue { ptr, i64 } %122, 1
  %125 = call ptr @sere_str_concat_data(ptr @115, i64 0, ptr %123, i64 %124, ptr %cat.len49)
  %126 = load i64, ptr %cat.len49, align 4
  %127 = insertvalue { ptr, i64 } undef, ptr %125, 0
  %128 = insertvalue { ptr, i64 } %127, i64 %126, 1
  store %Exception { i32 -1845790122, { ptr, i64 } zeroinitializer }, ptr %error, align 8
  call void @sere_error_copy_object(ptr %error, i64 24)
  %129 = load %Exception, ptr %error, align 8
  %130 = insertvalue %Exception %129, { ptr, i64 } %128, 1
  store %Exception %130, ptr %error, align 8
  call void @sere_error_enter()
  %131 = getelementptr inbounds nuw %Exception, ptr %error, i32 0, i32 1
  %132 = load { ptr, i64 }, ptr %131, align 8
  %133 = call %Response @wsgi_server_error({ ptr, i64 } %132)
  %134 = call i32 @sere_has_error()
  %135 = icmp ne i32 %134, 0
  br i1 %135, label %err50, label %err.ok51

try.next:                                         ; preds = %try.dispatch
  br label %try.finally

except.error:                                     ; preds = %err52, %err50
  call void @sere_error_leave(i32 0)
  br label %try.finally

err50:                                            ; preds = %try.except
  br label %except.error

err.ok51:                                         ; preds = %try.except
  %136 = call i32 @sere_has_error()
  %137 = icmp ne i32 %136, 0
  br i1 %137, label %err52, label %err.ok53

err52:                                            ; preds = %err.ok51
  br label %except.error

err.ok53:                                         ; preds = %err.ok51
  call void @sere_error_leave(i32 0)
  ret %Response %133

finally.error:                                    ; No predecessors!
  call void @sere_error_leave(i32 0)
  br label %try.after

err54:                                            ; preds = %try.after
  ret %Response zeroinitializer

err.ok55:                                         ; preds = %try.after
  ret %Response zeroinitializer
}

define void @wsgi_Server___init__(ptr %self, { ptr, i64 } %host, i32 %port) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %host1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %host, ptr %host1, align 8
  %port2 = alloca i32, align 4
  store i32 %port, ptr %port2, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Server, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %host1, align 8
  %3 = extractvalue { ptr, i64 } %2, 0
  %4 = extractvalue { ptr, i64 } %2, 1
  %5 = load i32, ptr %port2, align 4
  %6 = call ptr @sere_http_listen(ptr %3, i64 %4, i32 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store ptr %6, ptr %1, align 8
  ret void
}

define i1 @wsgi_Server_is_running(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Server, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  %4 = xor i1 %3, true
  %5 = call i32 @sere_has_error()
  %6 = icmp ne i32 %5, 0
  br i1 %6, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  ret i1 %4
}

define ptr @wsgi_Server_accept(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Server, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = call ptr @sere_http_accept(ptr %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret ptr null

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret ptr null

err.ok2:                                          ; preds = %err.ok
  ret ptr %3
}

define void @wsgi_Server_close(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Server, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  %4 = xor i1 %3, true
  br i1 %4, label %if.then, label %if.next

if.end:                                           ; preds = %if.next, %err.ok4
  ret void

if.then:                                          ; preds = %entry
  %5 = load ptr, ptr %self.slot, align 8
  %6 = getelementptr inbounds nuw %Server, ptr %5, i32 0, i32 1
  %7 = load ptr, ptr %6, align 8
  call void @sere_http_close(ptr %7)
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret void

err.ok:                                           ; preds = %if.then
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret void

err.ok2:                                          ; preds = %err.ok
  %12 = load ptr, ptr %self.slot, align 8
  %13 = getelementptr inbounds nuw %Server, ptr %12, i32 0, i32 1
  store ptr null, ptr %13, align 8
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok2
  ret void

err.ok4:                                          ; preds = %err.ok2
  br label %if.end
}

define %Environ @wsgi_request_environ(ptr %req) {
entry:
  %req1 = alloca ptr, align 8
  store ptr %req, ptr %req1, align 8
  %environ = alloca %Environ, align 8
  %init.tmp = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Environ, ptr %init.tmp, i32 0, i32 0
  store i32 -1407819286, ptr %0, align 4
  %1 = load ptr, ptr %req1, align 8
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_http_req_method(ptr %1, ptr %ext.str.data, ptr %ext.str.len)
  %2 = load i64, ptr %ext.str.len, align 4
  %3 = load ptr, ptr %ext.str.data, align 8
  %4 = insertvalue { ptr, i64 } undef, ptr %3, 0
  %5 = insertvalue { ptr, i64 } %4, i64 %2, 1
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Environ zeroinitializer

err.ok:                                           ; preds = %entry
  %8 = load ptr, ptr %req1, align 8
  %ext.str.data2 = alloca ptr, align 8
  %ext.str.len3 = alloca i64, align 8
  call void @sere_http_req_path(ptr %8, ptr %ext.str.data2, ptr %ext.str.len3)
  %9 = load i64, ptr %ext.str.len3, align 4
  %10 = load ptr, ptr %ext.str.data2, align 8
  %11 = insertvalue { ptr, i64 } undef, ptr %10, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %9, 1
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret %Environ zeroinitializer

err.ok5:                                          ; preds = %err.ok
  %15 = load ptr, ptr %req1, align 8
  %ext.str.data6 = alloca ptr, align 8
  %ext.str.len7 = alloca i64, align 8
  call void @sere_http_req_body(ptr %15, ptr %ext.str.data6, ptr %ext.str.len7)
  %16 = load i64, ptr %ext.str.len7, align 4
  %17 = load ptr, ptr %ext.str.data6, align 8
  %18 = insertvalue { ptr, i64 } undef, ptr %17, 0
  %19 = insertvalue { ptr, i64 } %18, i64 %16, 1
  %20 = call i32 @sere_has_error()
  %21 = icmp ne i32 %20, 0
  br i1 %21, label %err8, label %err.ok9

err8:                                             ; preds = %err.ok5
  ret %Environ zeroinitializer

err.ok9:                                          ; preds = %err.ok5
  call void @wsgi_Environ___init__(ptr %init.tmp, { ptr, i64 } %5, { ptr, i64 } %12, { ptr, i64 } %19)
  %22 = load %Environ, ptr %init.tmp, align 8
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err10, label %err.ok11

err10:                                            ; preds = %err.ok9
  ret %Environ zeroinitializer

err.ok11:                                         ; preds = %err.ok9
  store %Environ %22, ptr %environ, align 8
  %25 = getelementptr inbounds nuw %Environ, ptr %environ, i32 0, i32 3
  %26 = load ptr, ptr %req1, align 8
  %ext.str.data12 = alloca ptr, align 8
  %ext.str.len13 = alloca i64, align 8
  call void @sere_http_req_query(ptr %26, ptr %ext.str.data12, ptr %ext.str.len13)
  %27 = load i64, ptr %ext.str.len13, align 4
  %28 = load ptr, ptr %ext.str.data12, align 8
  %29 = insertvalue { ptr, i64 } undef, ptr %28, 0
  %30 = insertvalue { ptr, i64 } %29, i64 %27, 1
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err14, label %err.ok15

err14:                                            ; preds = %err.ok11
  ret %Environ zeroinitializer

err.ok15:                                         ; preds = %err.ok11
  store { ptr, i64 } %30, ptr %25, align 8
  %33 = getelementptr inbounds nuw %Environ, ptr %environ, i32 0, i32 4
  %34 = load ptr, ptr %req1, align 8
  %ext.str.data16 = alloca ptr, align 8
  %ext.str.len17 = alloca i64, align 8
  call void @sere_http_req_version(ptr %34, ptr %ext.str.data16, ptr %ext.str.len17)
  %35 = load i64, ptr %ext.str.len17, align 4
  %36 = load ptr, ptr %ext.str.data16, align 8
  %37 = insertvalue { ptr, i64 } undef, ptr %36, 0
  %38 = insertvalue { ptr, i64 } %37, i64 %35, 1
  %39 = call i32 @sere_has_error()
  %40 = icmp ne i32 %39, 0
  br i1 %40, label %err18, label %err.ok19

err18:                                            ; preds = %err.ok15
  ret %Environ zeroinitializer

err.ok19:                                         ; preds = %err.ok15
  store { ptr, i64 } %38, ptr %33, align 8
  %41 = getelementptr inbounds nuw %Environ, ptr %environ, i32 0, i32 6
  %42 = load ptr, ptr %req1, align 8
  %ext.str.data20 = alloca ptr, align 8
  %ext.str.len21 = alloca i64, align 8
  call void @sere_http_req_headers(ptr %42, ptr %ext.str.data20, ptr %ext.str.len21)
  %43 = load i64, ptr %ext.str.len21, align 4
  %44 = load ptr, ptr %ext.str.data20, align 8
  %45 = insertvalue { ptr, i64 } undef, ptr %44, 0
  %46 = insertvalue { ptr, i64 } %45, i64 %43, 1
  %47 = call i32 @sere_has_error()
  %48 = icmp ne i32 %47, 0
  br i1 %48, label %err22, label %err.ok23

err22:                                            ; preds = %err.ok19
  ret %Environ zeroinitializer

err.ok23:                                         ; preds = %err.ok19
  %49 = call %MultiMap @wsgi_parse_headers({ ptr, i64 } %46)
  %50 = call i32 @sere_has_error()
  %51 = icmp ne i32 %50, 0
  br i1 %51, label %err24, label %err.ok25

err24:                                            ; preds = %err.ok23
  ret %Environ zeroinitializer

err.ok25:                                         ; preds = %err.ok23
  store %MultiMap %49, ptr %41, align 8
  %52 = getelementptr inbounds nuw %Environ, ptr %environ, i32 0, i32 7
  %53 = getelementptr inbounds nuw %Environ, ptr %environ, i32 0, i32 3
  %54 = load { ptr, i64 }, ptr %53, align 8
  %55 = call %MultiMap @wsgi_parse_query({ ptr, i64 } %54)
  %56 = call i32 @sere_has_error()
  %57 = icmp ne i32 %56, 0
  br i1 %57, label %err26, label %err.ok27

err26:                                            ; preds = %err.ok25
  ret %Environ zeroinitializer

err.ok27:                                         ; preds = %err.ok25
  store %MultiMap %55, ptr %52, align 8
  %58 = load %Environ, ptr %environ, align 8
  %59 = call i32 @sere_has_error()
  %60 = icmp ne i32 %59, 0
  br i1 %60, label %err28, label %err.ok29

err28:                                            ; preds = %err.ok27
  ret %Environ zeroinitializer

err.ok29:                                         ; preds = %err.ok27
  ret %Environ %58
}

define %Response @wsgi_dispatch(ptr %app, %Environ %environ) {
entry:
  %error = alloca %Exception, align 8
  %app1 = alloca ptr, align 8
  store ptr %app, ptr %app1, align 8
  %environ2 = alloca %Environ, align 8
  store %Environ %environ, ptr %environ2, align 8
  %0 = load ptr, ptr %app1, align 8
  %1 = load %Environ, ptr %environ2, align 8
  %cb.fat = load { ptr, ptr }, ptr %0, align 8
  %cb.fn = extractvalue { ptr, ptr } %cb.fat, 0
  %cb.env = extractvalue { ptr, ptr } %cb.fat, 1
  %2 = icmp ne ptr %cb.env, null
  br i1 %2, label %cb.bound, label %cb.free

try.dispatch:                                     ; preds = %err3, %err
  %3 = call i32 @sere_error_isa(ptr @116)
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %try.except, label %try.next

try.finally:                                      ; preds = %try.next, %except.error
  call void @sere_error_enter()
  call void @sere_error_leave(i32 1)
  br label %try.after

try.after:                                        ; preds = %finally.error, %try.finally
  %5 = call i32 @sere_has_error()
  %6 = icmp ne i32 %5, 0
  br i1 %6, label %err9, label %err.ok10

cb.bound:                                         ; preds = %entry
  %cb.bound.ret = call %Response %cb.fn(ptr %cb.env, %Environ %1)
  br label %cb.merge

cb.free:                                          ; preds = %entry
  %cb.free.ret = call %Response %cb.fn(%Environ %1)
  br label %cb.merge

cb.merge:                                         ; preds = %cb.free, %cb.bound
  %cb.ret = phi %Response [ %cb.bound.ret, %cb.bound ], [ %cb.free.ret, %cb.free ]
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %cb.merge
  br label %try.dispatch

err.ok:                                           ; preds = %cb.merge
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  br label %try.dispatch

err.ok4:                                          ; preds = %err.ok
  ret %Response %cb.ret

try.except:                                       ; preds = %try.dispatch
  %11 = alloca i64, align 8
  %12 = call ptr @sere_error_message(ptr %11)
  %13 = load i64, ptr %11, align 4
  %14 = insertvalue { ptr, i64 } undef, ptr %12, 0
  %15 = insertvalue { ptr, i64 } %14, i64 %13, 1
  %cat.len = alloca i64, align 8
  %16 = extractvalue { ptr, i64 } %15, 0
  %17 = extractvalue { ptr, i64 } %15, 1
  %18 = call ptr @sere_str_concat_data(ptr @117, i64 0, ptr %16, i64 %17, ptr %cat.len)
  %19 = load i64, ptr %cat.len, align 4
  %20 = insertvalue { ptr, i64 } undef, ptr %18, 0
  %21 = insertvalue { ptr, i64 } %20, i64 %19, 1
  store %Exception { i32 -1845790122, { ptr, i64 } zeroinitializer }, ptr %error, align 8
  call void @sere_error_copy_object(ptr %error, i64 24)
  %22 = load %Exception, ptr %error, align 8
  %23 = insertvalue %Exception %22, { ptr, i64 } %21, 1
  store %Exception %23, ptr %error, align 8
  call void @sere_error_enter()
  %24 = getelementptr inbounds nuw %Exception, ptr %error, i32 0, i32 1
  %25 = load { ptr, i64 }, ptr %24, align 8
  %26 = call %Response @wsgi_server_error({ ptr, i64 } %25)
  %27 = call i32 @sere_has_error()
  %28 = icmp ne i32 %27, 0
  br i1 %28, label %err5, label %err.ok6

try.next:                                         ; preds = %try.dispatch
  br label %try.finally

except.error:                                     ; preds = %err7, %err5
  call void @sere_error_leave(i32 0)
  br label %try.finally

err5:                                             ; preds = %try.except
  br label %except.error

err.ok6:                                          ; preds = %try.except
  %29 = call i32 @sere_has_error()
  %30 = icmp ne i32 %29, 0
  br i1 %30, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  br label %except.error

err.ok8:                                          ; preds = %err.ok6
  call void @sere_error_leave(i32 0)
  ret %Response %26

finally.error:                                    ; No predecessors!
  call void @sere_error_leave(i32 0)
  br label %try.after

err9:                                             ; preds = %try.after
  ret %Response zeroinitializer

err.ok10:                                         ; preds = %try.after
  ret %Response zeroinitializer
}

define void @wsgi_send(ptr %req, %Response %response) {
entry:
  %req1 = alloca ptr, align 8
  store ptr %req, ptr %req1, align 8
  %response2 = alloca %Response, align 8
  store %Response %response, ptr %response2, align 8
  %extra = alloca { ptr, i64 }, align 8
  %0 = getelementptr inbounds nuw %Response, ptr %response2, i32 0, i32 5
  %1 = load %MultiMap, ptr %0, align 8
  %2 = call { ptr, i64 } @wsgi_headers_block(%MultiMap %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store { ptr, i64 } %2, ptr %extra, align 8
  %5 = load ptr, ptr %req1, align 8
  %6 = getelementptr inbounds nuw %Response, ptr %response2, i32 0, i32 1
  %7 = load i32, ptr %6, align 4
  %8 = getelementptr inbounds nuw %Response, ptr %response2, i32 0, i32 4
  %9 = load { ptr, i64 }, ptr %8, align 8
  %10 = extractvalue { ptr, i64 } %9, 0
  %11 = extractvalue { ptr, i64 } %9, 1
  %12 = getelementptr inbounds nuw %Response, ptr %response2, i32 0, i32 2
  %13 = load { ptr, i64 }, ptr %12, align 8
  %14 = extractvalue { ptr, i64 } %13, 0
  %15 = extractvalue { ptr, i64 } %13, 1
  %16 = load { ptr, i64 }, ptr %extra, align 8
  %17 = extractvalue { ptr, i64 } %16, 0
  %18 = extractvalue { ptr, i64 } %16, 1
  %19 = getelementptr inbounds nuw %Response, ptr %response2, i32 0, i32 3
  %20 = load { ptr, i64 }, ptr %19, align 8
  %21 = extractvalue { ptr, i64 } %20, 0
  %22 = extractvalue { ptr, i64 } %20, 1
  call void @sere_http_reply_ext(ptr %5, i32 %7, ptr %10, i64 %11, ptr %14, i64 %15, ptr %17, i64 %18, ptr %21, i64 %22)
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret void

err.ok4:                                          ; preds = %err.ok
  ret void
}

define void @wsgi_handle_request(ptr %app, ptr %req) {
entry:
  %app1 = alloca ptr, align 8
  store ptr %app, ptr %app1, align 8
  %req2 = alloca ptr, align 8
  store ptr %req, ptr %req2, align 8
  %environ = alloca %Environ, align 8
  %0 = load ptr, ptr %req2, align 8
  %1 = call %Environ @wsgi_request_environ(ptr %0)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store %Environ %1, ptr %environ, align 8
  %response = alloca %Response, align 8
  %4 = load ptr, ptr %app1, align 8
  %5 = load %Environ, ptr %environ, align 8
  %6 = call %Response @wsgi_dispatch(ptr %4, %Environ %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret void

err.ok4:                                          ; preds = %err.ok
  store %Response %6, ptr %response, align 8
  %9 = load ptr, ptr %req2, align 8
  %10 = load %Response, ptr %response, align 8
  call void @wsgi_send(ptr %9, %Response %10)
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret void

err.ok6:                                          ; preds = %err.ok4
  ret void
}

define i1 @wsgi_serve_once(ptr %app, %Server %server) {
entry:
  %app1 = alloca ptr, align 8
  store ptr %app, ptr %app1, align 8
  %server2 = alloca %Server, align 8
  store %Server %server, ptr %server2, align 8
  %req = alloca ptr, align 8
  %0 = call ptr @wsgi_Server_accept(ptr %server2)
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  store ptr %0, ptr %req, align 8
  %3 = load ptr, ptr %req, align 8
  %4 = icmp eq ptr %3, null
  br i1 %4, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %5 = load ptr, ptr %app1, align 8
  %6 = load ptr, ptr %req, align 8
  call void @wsgi_handle_request(ptr %5, ptr %6)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err5, label %err.ok6

if.then:                                          ; preds = %err.ok
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err3, label %err.ok4

if.next:                                          ; preds = %err.ok
  br label %if.end

err3:                                             ; preds = %if.then
  ret i1 false

err.ok4:                                          ; preds = %if.then
  ret i1 false

err5:                                             ; preds = %if.end
  ret i1 false

err.ok6:                                          ; preds = %if.end
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret i1 false

err.ok8:                                          ; preds = %err.ok6
  ret i1 true
}

define void @wsgi_serve(ptr %app, { ptr, i64 } %host, i32 %port) {
entry:
  %app1 = alloca ptr, align 8
  store ptr %app, ptr %app1, align 8
  %host2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %host, ptr %host2, align 8
  %port3 = alloca i32, align 4
  store i32 %port, ptr %port3, align 4
  %server = alloca %Server, align 8
  %init.tmp = alloca %Server, align 8
  store %Server zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Server, ptr %init.tmp, i32 0, i32 0
  store i32 1836253938, ptr %0, align 4
  %1 = load { ptr, i64 }, ptr %host2, align 8
  %2 = load i32, ptr %port3, align 4
  call void @wsgi_Server___init__(ptr %init.tmp, { ptr, i64 } %1, i32 %2)
  %3 = load %Server, ptr %init.tmp, align 8
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store %Server %3, ptr %server, align 8
  %6 = call i1 @wsgi_Server_is_running(ptr %server)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err4, label %err.ok5

if.end:                                           ; preds = %if.next
  br label %while.cond

err4:                                             ; preds = %err.ok
  ret void

err.ok5:                                          ; preds = %err.ok
  %9 = xor i1 %6, true
  br i1 %9, label %if.then, label %if.next

if.then:                                          ; preds = %err.ok5
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err6, label %err.ok7

if.next:                                          ; preds = %err.ok5
  br label %if.end

err6:                                             ; preds = %if.then
  ret void

err.ok7:                                          ; preds = %if.then
  ret void

while.cond:                                       ; preds = %err.ok20, %if.then13, %if.end
  br i1 true, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %req = alloca ptr, align 8
  %12 = call ptr @wsgi_Server_accept(ptr %server)
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err8, label %err.ok9

while.end:                                        ; preds = %while.cond
  ret void

err8:                                             ; preds = %while.body
  ret void

err.ok9:                                          ; preds = %while.body
  store ptr %12, ptr %req, align 8
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err10, label %err.ok11

err10:                                            ; preds = %err.ok9
  ret void

err.ok11:                                         ; preds = %err.ok9
  %17 = load ptr, ptr %req, align 8
  %18 = icmp eq ptr %17, null
  br i1 %18, label %if.then13, label %if.next14

if.end12:                                         ; preds = %if.next14
  %19 = call i32 @sere_has_error()
  %20 = icmp ne i32 %19, 0
  br i1 %20, label %err15, label %err.ok16

if.then13:                                        ; preds = %err.ok11
  br label %while.cond

if.next14:                                        ; preds = %err.ok11
  br label %if.end12

err15:                                            ; preds = %if.end12
  ret void

err.ok16:                                         ; preds = %if.end12
  %21 = load ptr, ptr %app1, align 8
  %22 = load ptr, ptr %req, align 8
  call void @wsgi_handle_request(ptr %21, ptr %22)
  %23 = call i32 @sere_has_error()
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %err17, label %err.ok18

err17:                                            ; preds = %err.ok16
  ret void

err.ok18:                                         ; preds = %err.ok16
  %25 = call i32 @sere_has_error()
  %26 = icmp ne i32 %25, 0
  br i1 %26, label %err19, label %err.ok20

err19:                                            ; preds = %err.ok18
  ret void

err.ok20:                                         ; preds = %err.ok18
  br label %while.cond
}

declare void @sere_string_upper(ptr, i64, ptr, ptr)

declare void @sere_string_lower(ptr, i64, ptr, ptr)

declare void @sere_string_strip(ptr, i64, ptr, ptr)

declare void @sere_string_repeat(ptr, i64, i64, ptr, ptr)

declare i32 @sere_string_starts_with(ptr, i64, ptr, i64)

declare i32 @sere_string_ends_with(ptr, i64, ptr, i64)

declare i32 @sere_str_contains(ptr, i64, ptr, i64)

declare i32 @sere_str_eq(ptr, i64, ptr, i64)

define i1 @string_starts_with({ ptr, i64 } %text, { ptr, i64 } %prefix) {
entry:
  %text1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %text, ptr %text1, align 8
  %prefix2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %prefix, ptr %prefix2, align 8
  %0 = load { ptr, i64 }, ptr %text1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = load { ptr, i64 }, ptr %prefix2, align 8
  %4 = extractvalue { ptr, i64 } %3, 0
  %5 = extractvalue { ptr, i64 } %3, 1
  %6 = call i32 @sere_string_starts_with(ptr %1, i64 %2, ptr %4, i64 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %9 = icmp ne i32 %6, 0
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret i1 false

err.ok4:                                          ; preds = %err.ok
  ret i1 %9
}

define i1 @string_ends_with({ ptr, i64 } %text, { ptr, i64 } %suffix) {
entry:
  %text1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %text, ptr %text1, align 8
  %suffix2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %suffix, ptr %suffix2, align 8
  %0 = load { ptr, i64 }, ptr %text1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = load { ptr, i64 }, ptr %suffix2, align 8
  %4 = extractvalue { ptr, i64 } %3, 0
  %5 = extractvalue { ptr, i64 } %3, 1
  %6 = call i32 @sere_string_ends_with(ptr %1, i64 %2, ptr %4, i64 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %9 = icmp ne i32 %6, 0
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret i1 false

err.ok4:                                          ; preds = %err.ok
  ret i1 %9
}

define i1 @string_contains({ ptr, i64 } %text, { ptr, i64 } %needle) {
entry:
  %text1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %text, ptr %text1, align 8
  %needle2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %needle, ptr %needle2, align 8
  %0 = load { ptr, i64 }, ptr %text1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = load { ptr, i64 }, ptr %needle2, align 8
  %4 = extractvalue { ptr, i64 } %3, 0
  %5 = extractvalue { ptr, i64 } %3, 1
  %6 = call i32 @sere_str_contains(ptr %1, i64 %2, ptr %4, i64 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %9 = icmp ne i32 %6, 0
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret i1 false

err.ok4:                                          ; preds = %err.ok
  ret i1 %9
}

define i1 @string_eq({ ptr, i64 } %left, { ptr, i64 } %right) {
entry:
  %left1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %left, ptr %left1, align 8
  %right2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %right, ptr %right2, align 8
  %0 = load { ptr, i64 }, ptr %left1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = load { ptr, i64 }, ptr %right2, align 8
  %4 = extractvalue { ptr, i64 } %3, 0
  %5 = extractvalue { ptr, i64 } %3, 1
  %6 = call i32 @sere_str_eq(ptr %1, i64 %2, ptr %4, i64 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %9 = icmp ne i32 %6, 0
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret i1 false

err.ok4:                                          ; preds = %err.ok
  ret i1 %9
}

declare i64 @sere_string_find(ptr, i64, ptr, i64)

declare void @sere_string_replace(ptr, i64, ptr, i64, ptr, i64, ptr, ptr)

declare ptr @sere_string_split(ptr, i64, ptr, i64)

declare void @sere_fs_read_text(ptr, i64, ptr, ptr)

declare i32 @sere_fs_write_text(ptr, i64, ptr, i64)

declare i32 @sere_fs_exists(ptr, i64)

declare i32 @sere_fs_is_file(ptr, i64)

declare i32 @sere_fs_is_dir(ptr, i64)

declare i32 @sere_fs_remove(ptr, i64)

declare i32 @sere_fs_mkdir(ptr, i64)

define i1 @fs_write_text({ ptr, i64 } %path, { ptr, i64 } %data) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %data2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %data, ptr %data2, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = load { ptr, i64 }, ptr %data2, align 8
  %4 = extractvalue { ptr, i64 } %3, 0
  %5 = extractvalue { ptr, i64 } %3, 1
  %6 = call i32 @sere_fs_write_text(ptr %1, i64 %2, ptr %4, i64 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %9 = icmp ne i32 %6, 0
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret i1 false

err.ok4:                                          ; preds = %err.ok
  ret i1 %9
}

define i1 @fs_exists({ ptr, i64 } %path) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_fs_exists(ptr %1, i64 %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %6 = icmp ne i32 %3, 0
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret i1 false

err.ok3:                                          ; preds = %err.ok
  ret i1 %6
}

define i1 @fs_is_file({ ptr, i64 } %path) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_fs_is_file(ptr %1, i64 %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %6 = icmp ne i32 %3, 0
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret i1 false

err.ok3:                                          ; preds = %err.ok
  ret i1 %6
}

define i1 @fs_is_dir({ ptr, i64 } %path) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_fs_is_dir(ptr %1, i64 %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %6 = icmp ne i32 %3, 0
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret i1 false

err.ok3:                                          ; preds = %err.ok
  ret i1 %6
}

define i1 @fs_remove({ ptr, i64 } %path) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_fs_remove(ptr %1, i64 %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %6 = icmp ne i32 %3, 0
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret i1 false

err.ok3:                                          ; preds = %err.ok
  ret i1 %6
}

define i1 @fs_mkdir({ ptr, i64 } %path) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %0 = load { ptr, i64 }, ptr %path1, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call i32 @sere_fs_mkdir(ptr %1, i64 %2)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  %6 = icmp ne i32 %3, 0
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret i1 false

err.ok3:                                          ; preds = %err.ok
  ret i1 %6
}

declare void @sere_input(ptr, i64, ptr, ptr)

define i32 @abs(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = icmp slt i32 %0, 0
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %2 = load i32, ptr %value1, align 4
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err2, label %err.ok3

if.then:                                          ; preds = %entry
  %5 = load i32, ptr %value1, align 4
  %6 = sub i32 0, %5
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %6

err2:                                             ; preds = %if.end
  ret i32 0

err.ok3:                                          ; preds = %if.end
  ret i32 %2
}

define i32 @min(i32 %left, i32 %right) {
entry:
  %left1 = alloca i32, align 4
  store i32 %left, ptr %left1, align 4
  %right2 = alloca i32, align 4
  store i32 %right, ptr %right2, align 4
  %0 = load i32, ptr %left1, align 4
  %1 = load i32, ptr %right2, align 4
  %2 = icmp slt i32 %0, %1
  br i1 %2, label %if.then, label %if.next

if.then:                                          ; preds = %entry
  %3 = load i32, ptr %left1, align 4
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %right2, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %3

err3:                                             ; preds = %if.next
  ret i32 0

err.ok4:                                          ; preds = %if.next
  ret i32 %6
}

define i32 @max(i32 %left, i32 %right) {
entry:
  %left1 = alloca i32, align 4
  store i32 %left, ptr %left1, align 4
  %right2 = alloca i32, align 4
  store i32 %right, ptr %right2, align 4
  %0 = load i32, ptr %left1, align 4
  %1 = load i32, ptr %right2, align 4
  %2 = icmp sgt i32 %0, %1
  br i1 %2, label %if.then, label %if.next

if.then:                                          ; preds = %entry
  %3 = load i32, ptr %left1, align 4
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %right2, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %3

err3:                                             ; preds = %if.next
  ret i32 0

err.ok4:                                          ; preds = %if.next
  ret i32 %6
}

define i32 @clamp(i32 %value, i32 %low, i32 %high) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %low2 = alloca i32, align 4
  store i32 %low, ptr %low2, align 4
  %high3 = alloca i32, align 4
  store i32 %high, ptr %high3, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = load i32, ptr %low2, align 4
  %2 = call i32 @max(i32 %0, i32 %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  %5 = load i32, ptr %high3, align 4
  %6 = call i32 @min(i32 %2, i32 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret i32 0

err.ok5:                                          ; preds = %err.ok
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err6, label %err.ok7

err6:                                             ; preds = %err.ok5
  ret i32 0

err.ok7:                                          ; preds = %err.ok5
  ret i32 %6
}

define i32 @sign(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next3
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err6, label %err.ok7

if.then:                                          ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %value1, align 4
  %7 = icmp slt i32 %6, 0
  br i1 %7, label %if.then2, label %if.next3

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 1

if.then2:                                         ; preds = %if.next
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err4, label %err.ok5

if.next3:                                         ; preds = %if.next
  br label %if.end

err4:                                             ; preds = %if.then2
  ret i32 0

err.ok5:                                          ; preds = %if.then2
  ret i32 -1

err6:                                             ; preds = %if.end
  ret i32 0

err.ok7:                                          ; preds = %if.end
  ret i32 0
}

define void @prelude_Exception___init__(ptr %self, { ptr, i64 } %message) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %message1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %message, ptr %message1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Exception, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %message1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  ret void
}

define %Response @home(%Environ %environ) {
entry:
  %environ1 = alloca %Environ, align 8
  store %Environ %environ, ptr %environ1, align 8
  %0 = call %Response @wsgi_text({ ptr, i64 } { ptr @118, i64 4 }, i32 200)
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Response %0
}

define %Response @greet(%Environ %environ) {
entry:
  %environ1 = alloca %Environ, align 8
  store %Environ %environ, ptr %environ1, align 8
  %0 = call { ptr, i64 } @wsgi_Environ_param(ptr %environ1, { ptr, i64 } { ptr @120, i64 4 }, { ptr, i64 } { ptr @121, i64 5 })
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %cat.len = alloca i64, align 8
  %3 = extractvalue { ptr, i64 } %0, 0
  %4 = extractvalue { ptr, i64 } %0, 1
  %5 = call ptr @sere_str_concat_data(ptr @119, i64 6, ptr %3, i64 %4, ptr %cat.len)
  %6 = load i64, ptr %cat.len, align 4
  %7 = insertvalue { ptr, i64 } undef, ptr %5, 0
  %8 = insertvalue { ptr, i64 } %7, i64 %6, 1
  %9 = call %Response @wsgi_text({ ptr, i64 } %8, i32 200)
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok3
  ret %Response zeroinitializer

err.ok5:                                          ; preds = %err.ok3
  ret %Response %9
}

define void @Counter___init__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Counter, ptr %0, i32 0, i32 1
  store i32 0, ptr %1, align 4
  ret void
}

define %Response @Counter_count(ptr %self, %Environ %environ) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %environ1 = alloca %Environ, align 8
  store %Environ %environ, ptr %environ1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Counter, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %self.slot, align 8
  %3 = getelementptr inbounds nuw %Counter, ptr %2, i32 0, i32 1
  %4 = load i32, ptr %3, align 4
  %5 = add i32 %4, 1
  store i32 %5, ptr %1, align 4
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Counter, ptr %6, i32 0, i32 1
  %8 = load i32, ptr %7, align 4
  %str.len = alloca i64, align 8
  %9 = call ptr @sere_str_i32_data(i32 %8, ptr %str.len)
  %10 = load i64, ptr %str.len, align 4
  %11 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %10, 1
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Response zeroinitializer

err.ok:                                           ; preds = %entry
  %cat.len = alloca i64, align 8
  %15 = extractvalue { ptr, i64 } %12, 0
  %16 = extractvalue { ptr, i64 } %12, 1
  %17 = call ptr @sere_str_concat_data(ptr @122, i64 5, ptr %15, i64 %16, ptr %cat.len)
  %18 = load i64, ptr %cat.len, align 4
  %19 = insertvalue { ptr, i64 } undef, ptr %17, 0
  %20 = insertvalue { ptr, i64 } %19, i64 %18, 1
  %21 = call %Response @wsgi_text({ ptr, i64 } %20, i32 200)
  %22 = call i32 @sere_has_error()
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Response zeroinitializer

err.ok3:                                          ; preds = %err.ok
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok3
  ret %Response zeroinitializer

err.ok5:                                          ; preds = %err.ok3
  ret %Response %21
}

define i32 @sere_main() {
entry:
  call void @sere.module.init()
  %router = alloca %Router, align 8
  %init.tmp = alloca %Router, align 8
  store %Router zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Router, ptr %init.tmp, i32 0, i32 0
  store i32 -1842425564, ptr %0, align 4
  call void @wsgi_Router___init__(ptr %init.tmp)
  %1 = load %Router, ptr %init.tmp, align 8
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  store %Router %1, ptr %router, align 8
  %cb.pack = call ptr @sere_alloc(i64 16)
  %4 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack, i32 0, i32 0
  store ptr @home, ptr %4, align 8
  %5 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack, i32 0, i32 1
  store ptr null, ptr %5, align 8
  call void @wsgi_Router_get(ptr %router, { ptr, i64 } { ptr @123, i64 1 }, ptr %cb.pack)
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i32 0

err.ok2:                                          ; preds = %err.ok
  %cb.pack3 = call ptr @sere_alloc(i64 16)
  %8 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack3, i32 0, i32 0
  store ptr @greet, ptr %8, align 8
  %9 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack3, i32 0, i32 1
  store ptr null, ptr %9, align 8
  call void @wsgi_Router_get(ptr %router, { ptr, i64 } { ptr @124, i64 6 }, ptr %cb.pack3)
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok2
  ret i32 0

err.ok5:                                          ; preds = %err.ok2
  %cb.pack6 = call ptr @sere_alloc(i64 16)
  %12 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack6, i32 0, i32 0
  store ptr @greet, ptr %12, align 8
  %13 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack6, i32 0, i32 1
  store ptr null, ptr %13, align 8
  call void @wsgi_Router_get(ptr %router, { ptr, i64 } { ptr @125, i64 11 }, ptr %cb.pack6)
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok5
  ret i32 0

err.ok8:                                          ; preds = %err.ok5
  %counter = alloca %Counter, align 8
  %init.tmp9 = alloca %Counter, align 8
  store %Counter zeroinitializer, ptr %init.tmp9, align 4
  %16 = getelementptr inbounds nuw %Counter, ptr %init.tmp9, i32 0, i32 0
  store i32 -66112573, ptr %16, align 4
  call void @Counter___init__(ptr %init.tmp9)
  %17 = load %Counter, ptr %init.tmp9, align 4
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err10, label %err.ok11

err10:                                            ; preds = %err.ok8
  ret i32 0

err.ok11:                                         ; preds = %err.ok8
  store %Counter %17, ptr %counter, align 4
  %cb.pack12 = call ptr @sere_alloc(i64 16)
  %20 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack12, i32 0, i32 0
  store ptr @Counter_count, ptr %20, align 8
  %21 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack12, i32 0, i32 1
  store ptr %counter, ptr %21, align 8
  call void @wsgi_Router_post(ptr %router, { ptr, i64 } { ptr @126, i64 6 }, ptr %cb.pack12)
  %22 = call i32 @sere_has_error()
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %err13, label %err.ok14

err13:                                            ; preds = %err.ok11
  ret i32 0

err.ok14:                                         ; preds = %err.ok11
  %env1 = alloca %Environ, align 8
  %init.tmp15 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp15, align 8
  %24 = getelementptr inbounds nuw %Environ, ptr %init.tmp15, i32 0, i32 0
  store i32 -1407819286, ptr %24, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp15, { ptr, i64 } { ptr @127, i64 3 }, { ptr, i64 } { ptr @128, i64 1 }, { ptr, i64 } { ptr @129, i64 0 })
  %25 = load %Environ, ptr %init.tmp15, align 8
  %26 = call i32 @sere_has_error()
  %27 = icmp ne i32 %26, 0
  br i1 %27, label %err16, label %err.ok17

err16:                                            ; preds = %err.ok14
  ret i32 0

err.ok17:                                         ; preds = %err.ok14
  store %Environ %25, ptr %env1, align 8
  %r1 = alloca %Response, align 8
  %28 = load %Environ, ptr %env1, align 8
  %29 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %28)
  %30 = call i32 @sere_has_error()
  %31 = icmp ne i32 %30, 0
  br i1 %31, label %err18, label %err.ok19

err18:                                            ; preds = %err.ok17
  ret i32 0

err.ok19:                                         ; preds = %err.ok17
  store %Response %29, ptr %r1, align 8
  %32 = getelementptr inbounds nuw %Response, ptr %r1, i32 0, i32 1
  %33 = load i32, ptr %32, align 4
  call void @sere_write_i32(i32 %33)
  call void @sere_write(ptr @130, i64 1)
  %34 = getelementptr inbounds nuw %Response, ptr %r1, i32 0, i32 3
  %35 = load { ptr, i64 }, ptr %34, align 8
  %36 = extractvalue { ptr, i64 } %35, 0
  %37 = extractvalue { ptr, i64 } %35, 1
  call void @sere_write(ptr %36, i64 %37)
  call void @sere_write_nl()
  %38 = call i32 @sere_has_error()
  %39 = icmp ne i32 %38, 0
  br i1 %39, label %err20, label %err.ok21

err20:                                            ; preds = %err.ok19
  ret i32 0

err.ok21:                                         ; preds = %err.ok19
  %env2 = alloca %Environ, align 8
  %init.tmp22 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp22, align 8
  %40 = getelementptr inbounds nuw %Environ, ptr %init.tmp22, i32 0, i32 0
  store i32 -1407819286, ptr %40, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp22, { ptr, i64 } { ptr @131, i64 3 }, { ptr, i64 } { ptr @132, i64 6 }, { ptr, i64 } { ptr @133, i64 0 })
  %41 = load %Environ, ptr %init.tmp22, align 8
  %42 = call i32 @sere_has_error()
  %43 = icmp ne i32 %42, 0
  br i1 %43, label %err23, label %err.ok24

err23:                                            ; preds = %err.ok21
  ret i32 0

err.ok24:                                         ; preds = %err.ok21
  store %Environ %41, ptr %env2, align 8
  %44 = getelementptr inbounds nuw %Environ, ptr %env2, i32 0, i32 7
  %45 = call %MultiMap @wsgi_parse_query({ ptr, i64 } { ptr @134, i64 9 })
  %46 = call i32 @sere_has_error()
  %47 = icmp ne i32 %46, 0
  br i1 %47, label %err25, label %err.ok26

err25:                                            ; preds = %err.ok24
  ret i32 0

err.ok26:                                         ; preds = %err.ok24
  store %MultiMap %45, ptr %44, align 8
  %r2 = alloca %Response, align 8
  %48 = load %Environ, ptr %env2, align 8
  %49 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %48)
  %50 = call i32 @sere_has_error()
  %51 = icmp ne i32 %50, 0
  br i1 %51, label %err27, label %err.ok28

err27:                                            ; preds = %err.ok26
  ret i32 0

err.ok28:                                         ; preds = %err.ok26
  store %Response %49, ptr %r2, align 8
  %52 = getelementptr inbounds nuw %Response, ptr %r2, i32 0, i32 1
  %53 = load i32, ptr %52, align 4
  call void @sere_write_i32(i32 %53)
  call void @sere_write(ptr @135, i64 1)
  %54 = getelementptr inbounds nuw %Response, ptr %r2, i32 0, i32 3
  %55 = load { ptr, i64 }, ptr %54, align 8
  %56 = extractvalue { ptr, i64 } %55, 0
  %57 = extractvalue { ptr, i64 } %55, 1
  call void @sere_write(ptr %56, i64 %57)
  call void @sere_write_nl()
  %58 = call i32 @sere_has_error()
  %59 = icmp ne i32 %58, 0
  br i1 %59, label %err29, label %err.ok30

err29:                                            ; preds = %err.ok28
  ret i32 0

err.ok30:                                         ; preds = %err.ok28
  %env3 = alloca %Environ, align 8
  %init.tmp31 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp31, align 8
  %60 = getelementptr inbounds nuw %Environ, ptr %init.tmp31, i32 0, i32 0
  store i32 -1407819286, ptr %60, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp31, { ptr, i64 } { ptr @136, i64 4 }, { ptr, i64 } { ptr @137, i64 6 }, { ptr, i64 } { ptr @138, i64 0 })
  %61 = load %Environ, ptr %init.tmp31, align 8
  %62 = call i32 @sere_has_error()
  %63 = icmp ne i32 %62, 0
  br i1 %63, label %err32, label %err.ok33

err32:                                            ; preds = %err.ok30
  ret i32 0

err.ok33:                                         ; preds = %err.ok30
  store %Environ %61, ptr %env3, align 8
  %r3 = alloca %Response, align 8
  %64 = load %Environ, ptr %env3, align 8
  %65 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %64)
  %66 = call i32 @sere_has_error()
  %67 = icmp ne i32 %66, 0
  br i1 %67, label %err34, label %err.ok35

err34:                                            ; preds = %err.ok33
  ret i32 0

err.ok35:                                         ; preds = %err.ok33
  store %Response %65, ptr %r3, align 8
  %r4 = alloca %Response, align 8
  %68 = load %Environ, ptr %env3, align 8
  %69 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %68)
  %70 = call i32 @sere_has_error()
  %71 = icmp ne i32 %70, 0
  br i1 %71, label %err36, label %err.ok37

err36:                                            ; preds = %err.ok35
  ret i32 0

err.ok37:                                         ; preds = %err.ok35
  store %Response %69, ptr %r4, align 8
  %72 = getelementptr inbounds nuw %Response, ptr %r3, i32 0, i32 3
  %73 = load { ptr, i64 }, ptr %72, align 8
  %74 = extractvalue { ptr, i64 } %73, 0
  %75 = extractvalue { ptr, i64 } %73, 1
  call void @sere_write(ptr %74, i64 %75)
  call void @sere_write(ptr @139, i64 1)
  %76 = getelementptr inbounds nuw %Response, ptr %r4, i32 0, i32 3
  %77 = load { ptr, i64 }, ptr %76, align 8
  %78 = extractvalue { ptr, i64 } %77, 0
  %79 = extractvalue { ptr, i64 } %77, 1
  call void @sere_write(ptr %78, i64 %79)
  call void @sere_write_nl()
  %80 = call i32 @sere_has_error()
  %81 = icmp ne i32 %80, 0
  br i1 %81, label %err38, label %err.ok39

err38:                                            ; preds = %err.ok37
  ret i32 0

err.ok39:                                         ; preds = %err.ok37
  %env4 = alloca %Environ, align 8
  %init.tmp40 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp40, align 8
  %82 = getelementptr inbounds nuw %Environ, ptr %init.tmp40, i32 0, i32 0
  store i32 -1407819286, ptr %82, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp40, { ptr, i64 } { ptr @140, i64 3 }, { ptr, i64 } { ptr @141, i64 5 }, { ptr, i64 } { ptr @142, i64 0 })
  %83 = load %Environ, ptr %init.tmp40, align 8
  %84 = call i32 @sere_has_error()
  %85 = icmp ne i32 %84, 0
  br i1 %85, label %err41, label %err.ok42

err41:                                            ; preds = %err.ok39
  ret i32 0

err.ok42:                                         ; preds = %err.ok39
  store %Environ %83, ptr %env4, align 8
  %r5 = alloca %Response, align 8
  %86 = load %Environ, ptr %env4, align 8
  %87 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %86)
  %88 = call i32 @sere_has_error()
  %89 = icmp ne i32 %88, 0
  br i1 %89, label %err43, label %err.ok44

err43:                                            ; preds = %err.ok42
  ret i32 0

err.ok44:                                         ; preds = %err.ok42
  store %Response %87, ptr %r5, align 8
  %90 = getelementptr inbounds nuw %Response, ptr %r5, i32 0, i32 1
  %91 = load i32, ptr %90, align 4
  call void @sere_write_i32(i32 %91)
  call void @sere_write(ptr @143, i64 1)
  %92 = call { ptr, i64 } @wsgi_Response_status_line(ptr %r5)
  %93 = call i32 @sere_has_error()
  %94 = icmp ne i32 %93, 0
  br i1 %94, label %err45, label %err.ok46

err45:                                            ; preds = %err.ok44
  ret i32 0

err.ok46:                                         ; preds = %err.ok44
  %95 = extractvalue { ptr, i64 } %92, 0
  %96 = extractvalue { ptr, i64 } %92, 1
  call void @sere_write(ptr %95, i64 %96)
  call void @sere_write_nl()
  %97 = call i32 @sere_has_error()
  %98 = icmp ne i32 %97, 0
  br i1 %98, label %err47, label %err.ok48

err47:                                            ; preds = %err.ok46
  ret i32 0

err.ok48:                                         ; preds = %err.ok46
  %hdrs = alloca %MultiMap, align 8
  %99 = call %MultiMap @wsgi_parse_headers({ ptr, i64 } { ptr @144, i64 32 })
  %100 = call i32 @sere_has_error()
  %101 = icmp ne i32 %100, 0
  br i1 %101, label %err49, label %err.ok50

err49:                                            ; preds = %err.ok48
  ret i32 0

err.ok50:                                         ; preds = %err.ok48
  store %MultiMap %99, ptr %hdrs, align 8
  %102 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %hdrs, { ptr, i64 } { ptr @146, i64 4 }, { ptr, i64 } { ptr @147, i64 0 })
  %103 = call i32 @sere_has_error()
  %104 = icmp ne i32 %103, 0
  br i1 %104, label %err51, label %err.ok52

err51:                                            ; preds = %err.ok50
  ret i32 0

err.ok52:                                         ; preds = %err.ok50
  %105 = extractvalue { ptr, i64 } %102, 0
  %106 = extractvalue { ptr, i64 } %102, 1
  call void @sere_write(ptr %105, i64 %106)
  call void @sere_write(ptr @145, i64 1)
  %107 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %hdrs, { ptr, i64 } { ptr @148, i64 6 }, { ptr, i64 } { ptr @149, i64 0 })
  %108 = call i32 @sere_has_error()
  %109 = icmp ne i32 %108, 0
  br i1 %109, label %err53, label %err.ok54

err53:                                            ; preds = %err.ok52
  ret i32 0

err.ok54:                                         ; preds = %err.ok52
  %110 = extractvalue { ptr, i64 } %107, 0
  %111 = extractvalue { ptr, i64 } %107, 1
  call void @sere_write(ptr %110, i64 %111)
  call void @sere_write(ptr @145, i64 1)
  %112 = call i64 @wsgi_MultiMap_count(ptr %hdrs)
  %113 = call i32 @sere_has_error()
  %114 = icmp ne i32 %113, 0
  br i1 %114, label %err55, label %err.ok56

err55:                                            ; preds = %err.ok54
  ret i32 0

err.ok56:                                         ; preds = %err.ok54
  call void @sere_write_i64(i64 %112)
  call void @sere_write_nl()
  %115 = call i32 @sere_has_error()
  %116 = icmp ne i32 %115, 0
  br i1 %116, label %err57, label %err.ok58

err57:                                            ; preds = %err.ok56
  ret i32 0

err.ok58:                                         ; preds = %err.ok56
  %117 = call { ptr, i64 } @wsgi_status_text(i32 404)
  %118 = call i32 @sere_has_error()
  %119 = icmp ne i32 %118, 0
  br i1 %119, label %err59, label %err.ok60

err59:                                            ; preds = %err.ok58
  ret i32 0

err.ok60:                                         ; preds = %err.ok58
  %120 = extractvalue { ptr, i64 } %117, 0
  %121 = extractvalue { ptr, i64 } %117, 1
  call void @sere_write(ptr %120, i64 %121)
  call void @sere_write(ptr @150, i64 1)
  %122 = call { ptr, i64 } @wsgi_json_string({ ptr, i64 } { ptr @151, i64 5 })
  %123 = call i32 @sere_has_error()
  %124 = icmp ne i32 %123, 0
  br i1 %124, label %err61, label %err.ok62

err61:                                            ; preds = %err.ok60
  ret i32 0

err.ok62:                                         ; preds = %err.ok60
  %125 = extractvalue { ptr, i64 } %122, 0
  %126 = extractvalue { ptr, i64 } %122, 1
  call void @sere_write(ptr %125, i64 %126)
  call void @sere_write_nl()
  %127 = call i32 @sere_has_error()
  %128 = icmp ne i32 %127, 0
  br i1 %128, label %err63, label %err.ok64

err63:                                            ; preds = %err.ok62
  ret i32 0

err.ok64:                                         ; preds = %err.ok62
  %resp = alloca %Response, align 8
  %129 = call %Response @wsgi_redirect({ ptr, i64 } { ptr @152, i64 5 }, i32 302)
  %130 = call i32 @sere_has_error()
  %131 = icmp ne i32 %130, 0
  br i1 %131, label %err65, label %err.ok66

err65:                                            ; preds = %err.ok64
  ret i32 0

err.ok66:                                         ; preds = %err.ok64
  store %Response %129, ptr %resp, align 8
  call void @wsgi_Response_set_cookie(ptr %resp, { ptr, i64 } { ptr @153, i64 7 }, { ptr, i64 } { ptr @154, i64 3 }, { ptr, i64 } { ptr @155, i64 1 })
  %132 = call i32 @sere_has_error()
  %133 = icmp ne i32 %132, 0
  br i1 %133, label %err67, label %err.ok68

err67:                                            ; preds = %err.ok66
  ret i32 0

err.ok68:                                         ; preds = %err.ok66
  %134 = getelementptr inbounds nuw %Response, ptr %resp, i32 0, i32 1
  %135 = load i32, ptr %134, align 4
  call void @sere_write_i32(i32 %135)
  call void @sere_write(ptr @156, i64 1)
  %136 = getelementptr inbounds nuw %Response, ptr %resp, i32 0, i32 5
  %137 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %136, { ptr, i64 } { ptr @157, i64 8 }, { ptr, i64 } { ptr @158, i64 0 })
  %138 = call i32 @sere_has_error()
  %139 = icmp ne i32 %138, 0
  br i1 %139, label %err69, label %err.ok70

err69:                                            ; preds = %err.ok68
  ret i32 0

err.ok70:                                         ; preds = %err.ok68
  %140 = extractvalue { ptr, i64 } %137, 0
  %141 = extractvalue { ptr, i64 } %137, 1
  call void @sere_write(ptr %140, i64 %141)
  call void @sere_write(ptr @156, i64 1)
  %142 = getelementptr inbounds nuw %Response, ptr %resp, i32 0, i32 5
  %143 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %142, { ptr, i64 } { ptr @159, i64 10 }, { ptr, i64 } { ptr @160, i64 0 })
  %144 = call i32 @sere_has_error()
  %145 = icmp ne i32 %144, 0
  br i1 %145, label %err71, label %err.ok72

err71:                                            ; preds = %err.ok70
  ret i32 0

err.ok72:                                         ; preds = %err.ok70
  %146 = extractvalue { ptr, i64 } %143, 0
  %147 = extractvalue { ptr, i64 } %143, 1
  call void @sere_write(ptr %146, i64 %147)
  call void @sere_write_nl()
  %148 = call i32 @sere_has_error()
  %149 = icmp ne i32 %148, 0
  br i1 %149, label %err73, label %err.ok74

err73:                                            ; preds = %err.ok72
  ret i32 0

err.ok74:                                         ; preds = %err.ok72
  %form = alloca %MultiMap, align 8
  %init.tmp75 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp75, align 8
  %150 = getelementptr inbounds nuw %Environ, ptr %init.tmp75, i32 0, i32 0
  store i32 -1407819286, ptr %150, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp75, { ptr, i64 } { ptr @161, i64 4 }, { ptr, i64 } { ptr @162, i64 2 }, { ptr, i64 } { ptr @163, i64 9 })
  %151 = load %Environ, ptr %init.tmp75, align 8
  %152 = call i32 @sere_has_error()
  %153 = icmp ne i32 %152, 0
  br i1 %153, label %err76, label %err.ok77

err76:                                            ; preds = %err.ok74
  ret i32 0

err.ok77:                                         ; preds = %err.ok74
  %this.tmp = alloca %Environ, align 8
  store %Environ %151, ptr %this.tmp, align 8
  %154 = call %MultiMap @wsgi_Environ_form(ptr %this.tmp)
  %155 = call i32 @sere_has_error()
  %156 = icmp ne i32 %155, 0
  br i1 %156, label %err78, label %err.ok79

err78:                                            ; preds = %err.ok77
  ret i32 0

err.ok79:                                         ; preds = %err.ok77
  store %MultiMap %154, ptr %form, align 8
  %157 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %form, { ptr, i64 } { ptr @165, i64 1 }, { ptr, i64 } { ptr @166, i64 0 })
  %158 = call i32 @sere_has_error()
  %159 = icmp ne i32 %158, 0
  br i1 %159, label %err80, label %err.ok81

err80:                                            ; preds = %err.ok79
  ret i32 0

err.ok81:                                         ; preds = %err.ok79
  %160 = extractvalue { ptr, i64 } %157, 0
  %161 = extractvalue { ptr, i64 } %157, 1
  call void @sere_write(ptr %160, i64 %161)
  call void @sere_write(ptr @164, i64 1)
  %162 = call { ptr, i64 } @wsgi_MultiMap_get(ptr %form, { ptr, i64 } { ptr @167, i64 1 }, { ptr, i64 } { ptr @168, i64 0 })
  %163 = call i32 @sere_has_error()
  %164 = icmp ne i32 %163, 0
  br i1 %164, label %err82, label %err.ok83

err82:                                            ; preds = %err.ok81
  ret i32 0

err.ok83:                                         ; preds = %err.ok81
  %165 = extractvalue { ptr, i64 } %162, 0
  %166 = extractvalue { ptr, i64 } %162, 1
  call void @sere_write(ptr %165, i64 %166)
  call void @sere_write_nl()
  %167 = call i32 @sere_has_error()
  %168 = icmp ne i32 %167, 0
  br i1 %168, label %err84, label %err.ok85

err84:                                            ; preds = %err.ok83
  ret i32 0

err.ok85:                                         ; preds = %err.ok83
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_url_decode(ptr @170, i64 7, ptr %ext.str.data, ptr %ext.str.len)
  %169 = load i64, ptr %ext.str.len, align 4
  %170 = load ptr, ptr %ext.str.data, align 8
  %171 = insertvalue { ptr, i64 } undef, ptr %170, 0
  %172 = insertvalue { ptr, i64 } %171, i64 %169, 1
  %173 = call i32 @sere_has_error()
  %174 = icmp ne i32 %173, 0
  br i1 %174, label %err86, label %err.ok87

err86:                                            ; preds = %err.ok85
  ret i32 0

err.ok87:                                         ; preds = %err.ok85
  %175 = extractvalue { ptr, i64 } %172, 0
  %176 = extractvalue { ptr, i64 } %172, 1
  call void @sere_write(ptr %175, i64 %176)
  call void @sere_write(ptr @169, i64 1)
  %177 = call { ptr, i64 } @wsgi_content_type_for({ ptr, i64 } { ptr @171, i64 6 })
  %178 = call i32 @sere_has_error()
  %179 = icmp ne i32 %178, 0
  br i1 %179, label %err88, label %err.ok89

err88:                                            ; preds = %err.ok87
  ret i32 0

err.ok89:                                         ; preds = %err.ok87
  %180 = extractvalue { ptr, i64 } %177, 0
  %181 = extractvalue { ptr, i64 } %177, 1
  call void @sere_write(ptr %180, i64 %181)
  call void @sere_write_nl()
  %182 = call i32 @sere_has_error()
  %183 = icmp ne i32 %182, 0
  br i1 %183, label %err90, label %err.ok91

err90:                                            ; preds = %err.ok89
  ret i32 0

err.ok91:                                         ; preds = %err.ok89
  %env5 = alloca %Environ, align 8
  %init.tmp92 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp92, align 8
  %184 = getelementptr inbounds nuw %Environ, ptr %init.tmp92, i32 0, i32 0
  store i32 -1407819286, ptr %184, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp92, { ptr, i64 } { ptr @172, i64 3 }, { ptr, i64 } { ptr @173, i64 9 }, { ptr, i64 } { ptr @174, i64 0 })
  %185 = load %Environ, ptr %init.tmp92, align 8
  %186 = call i32 @sere_has_error()
  %187 = icmp ne i32 %186, 0
  br i1 %187, label %err93, label %err.ok94

err93:                                            ; preds = %err.ok91
  ret i32 0

err.ok94:                                         ; preds = %err.ok91
  store %Environ %185, ptr %env5, align 8
  %r6 = alloca %Response, align 8
  %188 = load %Environ, ptr %env5, align 8
  %189 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %188)
  %190 = call i32 @sere_has_error()
  %191 = icmp ne i32 %190, 0
  br i1 %191, label %err95, label %err.ok96

err95:                                            ; preds = %err.ok94
  ret i32 0

err.ok96:                                         ; preds = %err.ok94
  store %Response %189, ptr %r6, align 8
  %192 = getelementptr inbounds nuw %Response, ptr %r6, i32 0, i32 1
  %193 = load i32, ptr %192, align 4
  call void @sere_write_i32(i32 %193)
  call void @sere_write(ptr @175, i64 1)
  %194 = call { ptr, i64 } @wsgi_Environ_param(ptr %env5, { ptr, i64 } { ptr @176, i64 2 }, { ptr, i64 } { ptr @177, i64 0 })
  %195 = call i32 @sere_has_error()
  %196 = icmp ne i32 %195, 0
  br i1 %196, label %err97, label %err.ok98

err97:                                            ; preds = %err.ok96
  ret i32 0

err.ok98:                                         ; preds = %err.ok96
  %197 = extractvalue { ptr, i64 } %194, 0
  %198 = extractvalue { ptr, i64 } %194, 1
  call void @sere_write(ptr %197, i64 %198)
  call void @sere_write_nl()
  %199 = call i32 @sere_has_error()
  %200 = icmp ne i32 %199, 0
  br i1 %200, label %err99, label %err.ok100

err99:                                            ; preds = %err.ok98
  ret i32 0

err.ok100:                                        ; preds = %err.ok98
  %cb.pack101 = call ptr @sere_alloc(i64 16)
  %201 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack101, i32 0, i32 0
  store ptr @home, ptr %201, align 8
  %202 = getelementptr inbounds nuw { ptr, ptr }, ptr %cb.pack101, i32 0, i32 1
  store ptr null, ptr %202, align 8
  call void @wsgi_Router_mount(ptr %router, { ptr, i64 } { ptr @178, i64 7 }, ptr %cb.pack101)
  %203 = call i32 @sere_has_error()
  %204 = icmp ne i32 %203, 0
  br i1 %204, label %err102, label %err.ok103

err102:                                           ; preds = %err.ok100
  ret i32 0

err.ok103:                                        ; preds = %err.ok100
  %env6 = alloca %Environ, align 8
  %init.tmp104 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp104, align 8
  %205 = getelementptr inbounds nuw %Environ, ptr %init.tmp104, i32 0, i32 0
  store i32 -1407819286, ptr %205, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp104, { ptr, i64 } { ptr @179, i64 3 }, { ptr, i64 } { ptr @180, i64 14 }, { ptr, i64 } { ptr @181, i64 0 })
  %206 = load %Environ, ptr %init.tmp104, align 8
  %207 = call i32 @sere_has_error()
  %208 = icmp ne i32 %207, 0
  br i1 %208, label %err105, label %err.ok106

err105:                                           ; preds = %err.ok103
  ret i32 0

err.ok106:                                        ; preds = %err.ok103
  store %Environ %206, ptr %env6, align 8
  %r7 = alloca %Response, align 8
  %209 = load %Environ, ptr %env6, align 8
  %210 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %209)
  %211 = call i32 @sere_has_error()
  %212 = icmp ne i32 %211, 0
  br i1 %212, label %err107, label %err.ok108

err107:                                           ; preds = %err.ok106
  ret i32 0

err.ok108:                                        ; preds = %err.ok106
  store %Response %210, ptr %r7, align 8
  %213 = getelementptr inbounds nuw %Response, ptr %r7, i32 0, i32 3
  %214 = load { ptr, i64 }, ptr %213, align 8
  %215 = extractvalue { ptr, i64 } %214, 0
  %216 = extractvalue { ptr, i64 } %214, 1
  call void @sere_write(ptr %215, i64 %216)
  call void @sere_write_nl()
  %217 = call i32 @sere_has_error()
  %218 = icmp ne i32 %217, 0
  br i1 %218, label %err109, label %err.ok110

err109:                                           ; preds = %err.ok108
  ret i32 0

err.ok110:                                        ; preds = %err.ok108
  %missing = alloca %Response, align 8
  %init.tmp111 = alloca %Environ, align 8
  store %Environ zeroinitializer, ptr %init.tmp111, align 8
  %219 = getelementptr inbounds nuw %Environ, ptr %init.tmp111, i32 0, i32 0
  store i32 -1407819286, ptr %219, align 4
  call void @wsgi_Environ___init__(ptr %init.tmp111, { ptr, i64 } { ptr @183, i64 3 }, { ptr, i64 } { ptr @184, i64 7 }, { ptr, i64 } { ptr @185, i64 0 })
  %220 = load %Environ, ptr %init.tmp111, align 8
  %221 = call i32 @sere_has_error()
  %222 = icmp ne i32 %221, 0
  br i1 %222, label %err112, label %err.ok113

err112:                                           ; preds = %err.ok110
  ret i32 0

err.ok113:                                        ; preds = %err.ok110
  %223 = call %Response @wsgi_Router_dispatch(ptr %router, %Environ %220)
  %224 = call i32 @sere_has_error()
  %225 = icmp ne i32 %224, 0
  br i1 %225, label %err114, label %err.ok115

err114:                                           ; preds = %err.ok113
  ret i32 0

err.ok115:                                        ; preds = %err.ok113
  store %Response %223, ptr %missing, align 8
  %226 = getelementptr inbounds nuw %Response, ptr %missing, i32 0, i32 3
  %227 = load { ptr, i64 }, ptr %226, align 8
  %228 = extractvalue { ptr, i64 } %227, 0
  %229 = extractvalue { ptr, i64 } %227, 1
  call void @sere_write(ptr %228, i64 %229)
  call void @sere_write_nl()
  %230 = call i32 @sere_has_error()
  %231 = icmp ne i32 %230, 0
  br i1 %231, label %err116, label %err.ok117

err116:                                           ; preds = %err.ok115
  ret i32 0

err.ok117:                                        ; preds = %err.ok115
  %232 = call i32 @sere_has_error()
  %233 = icmp ne i32 %232, 0
  br i1 %233, label %err118, label %err.ok119

err118:                                           ; preds = %err.ok117
  ret i32 0

err.ok119:                                        ; preds = %err.ok117
  ret i32 0
}

define internal void @sere.module.init() {
entry:
  %0 = load i8, ptr @sere.module.init.done, align 1
  %1 = icmp eq i8 %0, 0
  br i1 %1, label %init, label %end

init:                                             ; preds = %entry
  store i8 1, ptr @sere.module.init.done, align 1
  br label %end

end:                                              ; preds = %init, %entry
  ret void
}

declare ptr @sere_alloc(i64)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare ptr @sere_list_new(i64)

declare void @sere_list_push(ptr, ptr)

declare i32 @sere_has_error()

declare void @sere_free(ptr)

declare void @sere_shared_release(ptr)

declare ptr @sere_list_item(ptr, i64)

declare i32 @sere_str_cmp(ptr, i64, ptr, i64)

declare void @sere_str_slice(ptr, i64, i64, i64, i32, i32, ptr, ptr)

declare ptr @sere_str_concat_data(ptr, i64, ptr, i64, ptr)

declare void @sere_str_index(ptr, i64, i64, ptr, ptr)

declare ptr @sere_str_i32_data(i32, ptr)

declare void @sere_error_enter()

declare void @sere_error_leave(i32)

declare i32 @sere_error_isa(ptr)

declare ptr @sere_error_message(ptr)

declare void @sere_error_copy_object(ptr, i64)

declare void @sere_write_nl()

declare void @sere_write_i32(i32)

declare void @sere_write(ptr, i64)

declare void @sere_write_i64(i64)

define i32 @main(i32 %0, ptr %1) {
entry:
  call void @sere_mod_init()
  call void @sere.module.init()
  %2 = call i32 @sere_main()
  call void @sere_error_unhandled()
  ret i32 %2
}

define weak void @sere_mod_init() {
entry:
  ret void
}

declare void @sere_error_unhandled()
