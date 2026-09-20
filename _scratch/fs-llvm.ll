; ModuleID = 'tests/backend_fs.sere'
source_filename = "tests/backend_fs.sere"
target triple = "x86_64-pc-windows-msvc"

%File = type { i32, ptr, { ptr, i64 }, { ptr, i64 } }
%Exception = type { i32, { ptr, i64 } }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@1 = private unnamed_addr constant [15 x i8] c"<closed file '\00", align 1
@2 = private unnamed_addr constant [9 x i8] c"' mode '\00", align 1
@3 = private unnamed_addr constant [3 x i8] c"'>\00", align 1
@4 = private unnamed_addr constant [13 x i8] c"<open file '\00", align 1
@5 = private unnamed_addr constant [9 x i8] c"' mode '\00", align 1
@6 = private unnamed_addr constant [3 x i8] c"'>\00", align 1
@7 = private unnamed_addr constant [8 x i8] c"Input: \00", align 1
@8 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@9 = private unnamed_addr constant [6 x i8] c"aeiou\00", align 1
@10 = private unnamed_addr constant [2 x i8] c" \00", align 1
@11 = private unnamed_addr constant [7 x i8] c"Found \00", align 1
@12 = private unnamed_addr constant [8 x i8] c" vowels\00", align 1
@13 = private unnamed_addr constant [2 x i8] c" \00", align 1
@14 = private unnamed_addr constant [2 x i8] c"[\00", align 1
@15 = private unnamed_addr constant [3 x i8] c", \00", align 1
@16 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@17 = private unnamed_addr constant [2 x i8] c"]\00", align 1
@18 = private unnamed_addr constant [11 x i8] c"output.txt\00", align 1
@19 = private unnamed_addr constant [2 x i8] c"w\00", align 1
@20 = private unnamed_addr constant [9 x i8] c"Vowels: \00", align 1
@21 = private unnamed_addr constant [11 x i8] c"output.txt\00", align 1
@22 = private unnamed_addr constant [2 x i8] c"r\00", align 1
@23 = private unnamed_addr constant [2 x i8] c" \00", align 1
@24 = private unnamed_addr constant [11 x i8] c"output.txt\00", align 1
@25 = private unnamed_addr constant [3 x i8] c"rb\00", align 1
@26 = private unnamed_addr constant [2 x i8] c" \00", align 1
@27 = private unnamed_addr constant [2 x i8] c"[\00", align 1
@28 = private unnamed_addr constant [3 x i8] c", \00", align 1
@29 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@30 = private unnamed_addr constant [2 x i8] c"]\00", align 1
@31 = private unnamed_addr constant [10 x i8] c"bytes.bin\00", align 1
@32 = private unnamed_addr constant [3 x i8] c"wb\00", align 1
@33 = private unnamed_addr constant [2 x i8] c" \00", align 1
@34 = private unnamed_addr constant [10 x i8] c"bytes.bin\00", align 1
@35 = private unnamed_addr constant [3 x i8] c"rb\00", align 1
@36 = private unnamed_addr constant [2 x i8] c" \00", align 1
@37 = private unnamed_addr constant [2 x i8] c" \00", align 1
@38 = private unnamed_addr constant [2 x i8] c"[\00", align 1
@39 = private unnamed_addr constant [3 x i8] c", \00", align 1
@40 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@41 = private unnamed_addr constant [2 x i8] c"]\00", align 1
@42 = private unnamed_addr constant [2 x i8] c" \00", align 1
@43 = private unnamed_addr constant [2 x i8] c" \00", align 1

declare void @sere_fs_read_text(ptr, i64, ptr, ptr)

declare i32 @sere_fs_write_text(ptr, i64, ptr, i64)

declare i32 @sere_fs_exists(ptr, i64)

declare i32 @sere_fs_is_file(ptr, i64)

declare i32 @sere_fs_is_dir(ptr, i64)

declare i32 @sere_fs_remove(ptr, i64)

declare i32 @sere_fs_mkdir(ptr, i64)

declare ptr @sere_fs_open(ptr, i64, ptr, i64)

declare i32 @sere_fs_close(ptr)

declare void @sere_fs_read_all(ptr, ptr, ptr)

declare i32 @sere_fs_write_all(ptr, ptr, i64)

declare i32 @sere_fs_read_bytes(ptr, ptr, i64)

declare i32 @sere_fs_write_bytes(ptr, ptr, i64)

declare void @sere_fs_seek(ptr, i64, i32)

define void @fs_File___init__(ptr %self, { ptr, i64 } %path, { ptr, i64 } %mode) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %mode2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %mode, ptr %mode2, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 3
  %2 = load { ptr, i64 }, ptr %path1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %File, ptr %3, i32 0, i32 2
  %5 = load { ptr, i64 }, ptr %mode2, align 8
  store { ptr, i64 } %5, ptr %4, align 8
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %File, ptr %6, i32 0, i32 1
  %8 = load { ptr, i64 }, ptr %path1, align 8
  %9 = extractvalue { ptr, i64 } %8, 0
  %10 = extractvalue { ptr, i64 } %8, 1
  %11 = load { ptr, i64 }, ptr %mode2, align 8
  %12 = extractvalue { ptr, i64 } %11, 0
  %13 = extractvalue { ptr, i64 } %11, 1
  %14 = call ptr @sere_fs_open(ptr %9, i64 %10, ptr %12, i64 %13)
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  store ptr %14, ptr %7, align 8
  ret void
}

define { ptr, i64 } @fs_File_read(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %4 = load ptr, ptr %self.slot, align 8
  %5 = getelementptr inbounds nuw %File, ptr %4, i32 0, i32 1
  %6 = load ptr, ptr %5, align 8
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_fs_read_all(ptr %6, ptr %ext.str.data, ptr %ext.str.len)
  %7 = load i64, ptr %ext.str.len, align 4
  %8 = load ptr, ptr %ext.str.data, align 8
  %9 = insertvalue { ptr, i64 } undef, ptr %8, 0
  %10 = insertvalue { ptr, i64 } %9, i64 %7, 1
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err1, label %err.ok2

if.then:                                          ; preds = %entry
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %if.then
  ret { ptr, i64 } { ptr @0, i64 0 }

err1:                                             ; preds = %if.end
  ret { ptr, i64 } zeroinitializer

err.ok2:                                          ; preds = %if.end
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok2
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %err.ok2
  ret { ptr, i64 } %10
}

define i1 @fs_File_write(ptr %self, { ptr, i64 } %data) {
entry:
  %result = alloca i32, align 4
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %data1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %data, ptr %data1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %4 = load ptr, ptr %self.slot, align 8
  %5 = getelementptr inbounds nuw %File, ptr %4, i32 0, i32 1
  %6 = load ptr, ptr %5, align 8
  %7 = load { ptr, i64 }, ptr %data1, align 8
  %8 = extractvalue { ptr, i64 } %7, 0
  %9 = extractvalue { ptr, i64 } %7, 1
  %10 = call i32 @sere_fs_write_all(ptr %6, ptr %8, i64 %9)
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err2, label %err.ok3

if.then:                                          ; preds = %entry
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret i1 false

err.ok:                                           ; preds = %if.then
  ret i1 false

err2:                                             ; preds = %if.end
  ret i1 false

err.ok3:                                          ; preds = %if.end
  store i32 %10, ptr %result, align 4
  %15 = load i32, ptr %result, align 4
  %16 = icmp ne i32 %15, 0
  %17 = call i32 @sere_has_error()
  %18 = icmp ne i32 %17, 0
  br i1 %18, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok3
  ret i1 false

err.ok5:                                          ; preds = %err.ok3
  ret i1 %16
}

define ptr @fs_File_read_bytes(ptr %self, i64 %buffer_size) {
entry:
  %bytes_read = alloca i32, align 4
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %buffer_size1 = alloca i64, align 8
  store i64 %buffer_size, ptr %buffer_size1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %buffer = alloca ptr, align 8
  %4 = call ptr @sere_array_new(i64 1, i64 1)
  %5 = call ptr @sere_list_item(ptr %4, i64 0)
  %6 = load i64, ptr %buffer_size1, align 4
  %7 = trunc i64 %6 to i8
  store i8 %7, ptr %5, align 1
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err2, label %err.ok3

if.then:                                          ; preds = %entry
  %10 = call ptr @sere_list_new(i64 1)
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret ptr null

err.ok:                                           ; preds = %if.then
  ret ptr %10

err2:                                             ; preds = %if.end
  ret ptr null

err.ok3:                                          ; preds = %if.end
  store ptr %4, ptr %buffer, align 8
  %13 = load ptr, ptr %self.slot, align 8
  %14 = getelementptr inbounds nuw %File, ptr %13, i32 0, i32 1
  %15 = load ptr, ptr %14, align 8
  %16 = load ptr, ptr %buffer, align 8
  %17 = call ptr @sere_list_item(ptr %16, i64 0)
  %18 = load i64, ptr %buffer_size1, align 4
  %19 = call i32 @sere_fs_read_bytes(ptr %15, ptr %17, i64 %18)
  %20 = call i32 @sere_has_error()
  %21 = icmp ne i32 %20, 0
  br i1 %21, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok3
  ret ptr null

err.ok5:                                          ; preds = %err.ok3
  store i32 %19, ptr %bytes_read, align 4
  %result = alloca ptr, align 8
  %22 = call ptr @sere_list_new(i64 1)
  store ptr %22, ptr %result, align 8
  %23 = load i32, ptr %bytes_read, align 4
  %24 = icmp sgt i32 %23, 0
  br i1 %24, label %if.then7, label %if.next8

if.end6:                                          ; preds = %if.next8, %err.ok14
  %25 = load ptr, ptr %result, align 8
  %26 = call i32 @sere_has_error()
  %27 = icmp ne i32 %26, 0
  br i1 %27, label %err15, label %err.ok16

if.then7:                                         ; preds = %err.ok5
  %28 = load i32, ptr %bytes_read, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %for.cond

if.next8:                                         ; preds = %err.ok5
  br label %if.end6

for.cond:                                         ; preds = %for.next, %if.then7
  %29 = load i32, ptr %i, align 4
  %30 = icmp slt i32 %29, %28
  %31 = icmp sgt i32 %29, %28
  %32 = select i1 true, i1 %30, i1 %31
  br i1 %32, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %33 = load ptr, ptr %result, align 8
  %34 = load ptr, ptr %result, align 8
  %35 = load ptr, ptr %buffer, align 8
  %36 = load i32, ptr %i, align 4
  %37 = sext i32 %36 to i64
  %38 = call ptr @sere_list_item(ptr %35, i64 %37)
  %39 = load i8, ptr %38, align 1
  %tmp.slot = alloca i8, align 1
  store i8 %39, ptr %tmp.slot, align 1
  call void @sere_list_push(ptr %34, ptr %tmp.slot)
  %40 = call i32 @sere_has_error()
  %41 = icmp ne i32 %40, 0
  br i1 %41, label %err9, label %err.ok10

for.end:                                          ; preds = %for.cond
  %42 = call i32 @sere_has_error()
  %43 = icmp ne i32 %42, 0
  br i1 %43, label %err13, label %err.ok14

for.next:                                         ; preds = %err.ok12
  %44 = load i32, ptr %i, align 4
  %45 = add i32 %44, 1
  store i32 %45, ptr %i, align 4
  br label %for.cond

err9:                                             ; preds = %for.body
  ret ptr null

err.ok10:                                         ; preds = %for.body
  %46 = call i32 @sere_has_error()
  %47 = icmp ne i32 %46, 0
  br i1 %47, label %err11, label %err.ok12

err11:                                            ; preds = %err.ok10
  ret ptr null

err.ok12:                                         ; preds = %err.ok10
  br label %for.next

err13:                                            ; preds = %for.end
  ret ptr null

err.ok14:                                         ; preds = %for.end
  br label %if.end6

err15:                                            ; preds = %if.end6
  ret ptr null

err.ok16:                                         ; preds = %if.end6
  ret ptr %25
}

define i1 @fs_File_write_bytes(ptr %self, ptr %data) {
entry:
  %result = alloca i32, align 4
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %data1 = alloca ptr, align 8
  store ptr %data, ptr %data1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %log.end, label %log.rhs

if.end:                                           ; preds = %if.next
  %4 = load ptr, ptr %self.slot, align 8
  %5 = getelementptr inbounds nuw %File, ptr %4, i32 0, i32 1
  %6 = load ptr, ptr %5, align 8
  %7 = load ptr, ptr %data1, align 8
  %8 = call ptr @sere_list_item(ptr %7, i64 0)
  %9 = load ptr, ptr %data1, align 8
  %10 = call i64 @sere_list_len(ptr %9)
  %11 = call i32 @sere_has_error()
  %12 = icmp ne i32 %11, 0
  br i1 %12, label %err4, label %err.ok5

log.rhs:                                          ; preds = %entry
  %13 = load ptr, ptr %data1, align 8
  %14 = call i64 @sere_list_len(ptr %13)
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err, label %err.ok

log.end:                                          ; preds = %err.ok, %entry
  %log.phi = phi i1 [ %3, %entry ], [ %17, %err.ok ]
  br i1 %log.phi, label %if.then, label %if.next

err:                                              ; preds = %log.rhs
  ret i1 false

err.ok:                                           ; preds = %log.rhs
  %17 = icmp eq i64 %14, 0
  br label %log.end

if.then:                                          ; preds = %log.end
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err2, label %err.ok3

if.next:                                          ; preds = %log.end
  br label %if.end

err2:                                             ; preds = %if.then
  ret i1 false

err.ok3:                                          ; preds = %if.then
  ret i1 false

err4:                                             ; preds = %if.end
  ret i1 false

err.ok5:                                          ; preds = %if.end
  %20 = call i32 @sere_fs_write_bytes(ptr %6, ptr %8, i64 %10)
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err6, label %err.ok7

err6:                                             ; preds = %err.ok5
  ret i1 false

err.ok7:                                          ; preds = %err.ok5
  store i32 %20, ptr %result, align 4
  %23 = load i32, ptr %result, align 4
  %24 = icmp ne i32 %23, 0
  %25 = call i32 @sere_has_error()
  %26 = icmp ne i32 %25, 0
  br i1 %26, label %err8, label %err.ok9

err8:                                             ; preds = %err.ok7
  ret i1 false

err.ok9:                                          ; preds = %err.ok7
  ret i1 %24
}

define void @fs_File_seek(ptr %self, i64 %offset, i32 %whence) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %offset1 = alloca i64, align 8
  store i64 %offset, ptr %offset1, align 4
  %whence2 = alloca i32, align 4
  store i32 %whence, ptr %whence2, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  %4 = xor i1 %3, true
  br i1 %4, label %if.then, label %if.next

if.end:                                           ; preds = %if.next, %err.ok4
  ret void

if.then:                                          ; preds = %entry
  %5 = load ptr, ptr %self.slot, align 8
  %6 = getelementptr inbounds nuw %File, ptr %5, i32 0, i32 1
  %7 = load ptr, ptr %6, align 8
  %8 = load i64, ptr %offset1, align 4
  %9 = load i32, ptr %whence2, align 4
  call void @sere_fs_seek(ptr %7, i64 %8, i32 %9)
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret void

err.ok:                                           ; preds = %if.then
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret void

err.ok4:                                          ; preds = %err.ok
  br label %if.end
}

define i1 @fs_File_close(ptr %self) {
entry:
  %result = alloca i1, align 1
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %4 = load ptr, ptr %self.slot, align 8
  %5 = getelementptr inbounds nuw %File, ptr %4, i32 0, i32 1
  %6 = load ptr, ptr %5, align 8
  %7 = call i32 @sere_fs_close(ptr %6)
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err1, label %err.ok2

if.then:                                          ; preds = %entry
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret i1 false

err.ok:                                           ; preds = %if.then
  ret i1 false

err1:                                             ; preds = %if.end
  ret i1 false

err.ok2:                                          ; preds = %if.end
  %12 = icmp ne i32 %7, 0
  store i1 %12, ptr %result, align 1
  %13 = load i1, ptr %result, align 1
  br i1 %13, label %if.then4, label %if.next5

if.end3:                                          ; preds = %if.next5, %err.ok7
  %14 = load i1, ptr %result, align 1
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err8, label %err.ok9

if.then4:                                         ; preds = %err.ok2
  %17 = load ptr, ptr %self.slot, align 8
  %18 = getelementptr inbounds nuw %File, ptr %17, i32 0, i32 1
  store ptr null, ptr %18, align 8
  %19 = call i32 @sere_has_error()
  %20 = icmp ne i32 %19, 0
  br i1 %20, label %err6, label %err.ok7

if.next5:                                         ; preds = %err.ok2
  br label %if.end3

err6:                                             ; preds = %if.then4
  ret i1 false

err.ok7:                                          ; preds = %if.then4
  br label %if.end3

err8:                                             ; preds = %if.end3
  ret i1 false

err.ok9:                                          ; preds = %if.end3
  ret i1 %14
}

define %File @fs_File___enter__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = load %File, ptr %0, align 8
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %File zeroinitializer

err.ok:                                           ; preds = %entry
  ret %File %1
}

define void @fs_File___exit__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = call i1 @fs_File_close(ptr %0)
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret void

err.ok:                                           ; preds = %entry
  ret void
}

define { ptr, i64 } @fs_File___repr__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %File, ptr %0, i32 0, i32 1
  %2 = load ptr, ptr %1, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %4 = load ptr, ptr %self.slot, align 8
  %5 = getelementptr inbounds nuw %File, ptr %4, i32 0, i32 3
  %6 = load { ptr, i64 }, ptr %5, align 8
  %cat.len4 = alloca i64, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  %9 = call ptr @sere_str_concat_data(ptr @4, i64 12, ptr %7, i64 %8, ptr %cat.len4)
  %10 = load i64, ptr %cat.len4, align 4
  %11 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %10, 1
  %cat.len5 = alloca i64, align 8
  %13 = extractvalue { ptr, i64 } %12, 0
  %14 = extractvalue { ptr, i64 } %12, 1
  %15 = call ptr @sere_str_concat_data(ptr %13, i64 %14, ptr @5, i64 8, ptr %cat.len5)
  %16 = load i64, ptr %cat.len5, align 4
  %17 = insertvalue { ptr, i64 } undef, ptr %15, 0
  %18 = insertvalue { ptr, i64 } %17, i64 %16, 1
  %19 = load ptr, ptr %self.slot, align 8
  %20 = getelementptr inbounds nuw %File, ptr %19, i32 0, i32 2
  %21 = load { ptr, i64 }, ptr %20, align 8
  %cat.len6 = alloca i64, align 8
  %22 = extractvalue { ptr, i64 } %18, 0
  %23 = extractvalue { ptr, i64 } %18, 1
  %24 = extractvalue { ptr, i64 } %21, 0
  %25 = extractvalue { ptr, i64 } %21, 1
  %26 = call ptr @sere_str_concat_data(ptr %22, i64 %23, ptr %24, i64 %25, ptr %cat.len6)
  %27 = load i64, ptr %cat.len6, align 4
  %28 = insertvalue { ptr, i64 } undef, ptr %26, 0
  %29 = insertvalue { ptr, i64 } %28, i64 %27, 1
  %cat.len7 = alloca i64, align 8
  %30 = extractvalue { ptr, i64 } %29, 0
  %31 = extractvalue { ptr, i64 } %29, 1
  %32 = call ptr @sere_str_concat_data(ptr %30, i64 %31, ptr @6, i64 2, ptr %cat.len7)
  %33 = load i64, ptr %cat.len7, align 4
  %34 = insertvalue { ptr, i64 } undef, ptr %32, 0
  %35 = insertvalue { ptr, i64 } %34, i64 %33, 1
  %36 = call i32 @sere_has_error()
  %37 = icmp ne i32 %36, 0
  br i1 %37, label %err8, label %err.ok9

if.then:                                          ; preds = %entry
  %38 = load ptr, ptr %self.slot, align 8
  %39 = getelementptr inbounds nuw %File, ptr %38, i32 0, i32 3
  %40 = load { ptr, i64 }, ptr %39, align 8
  %cat.len = alloca i64, align 8
  %41 = extractvalue { ptr, i64 } %40, 0
  %42 = extractvalue { ptr, i64 } %40, 1
  %43 = call ptr @sere_str_concat_data(ptr @1, i64 14, ptr %41, i64 %42, ptr %cat.len)
  %44 = load i64, ptr %cat.len, align 4
  %45 = insertvalue { ptr, i64 } undef, ptr %43, 0
  %46 = insertvalue { ptr, i64 } %45, i64 %44, 1
  %cat.len1 = alloca i64, align 8
  %47 = extractvalue { ptr, i64 } %46, 0
  %48 = extractvalue { ptr, i64 } %46, 1
  %49 = call ptr @sere_str_concat_data(ptr %47, i64 %48, ptr @2, i64 8, ptr %cat.len1)
  %50 = load i64, ptr %cat.len1, align 4
  %51 = insertvalue { ptr, i64 } undef, ptr %49, 0
  %52 = insertvalue { ptr, i64 } %51, i64 %50, 1
  %53 = load ptr, ptr %self.slot, align 8
  %54 = getelementptr inbounds nuw %File, ptr %53, i32 0, i32 2
  %55 = load { ptr, i64 }, ptr %54, align 8
  %cat.len2 = alloca i64, align 8
  %56 = extractvalue { ptr, i64 } %52, 0
  %57 = extractvalue { ptr, i64 } %52, 1
  %58 = extractvalue { ptr, i64 } %55, 0
  %59 = extractvalue { ptr, i64 } %55, 1
  %60 = call ptr @sere_str_concat_data(ptr %56, i64 %57, ptr %58, i64 %59, ptr %cat.len2)
  %61 = load i64, ptr %cat.len2, align 4
  %62 = insertvalue { ptr, i64 } undef, ptr %60, 0
  %63 = insertvalue { ptr, i64 } %62, i64 %61, 1
  %cat.len3 = alloca i64, align 8
  %64 = extractvalue { ptr, i64 } %63, 0
  %65 = extractvalue { ptr, i64 } %63, 1
  %66 = call ptr @sere_str_concat_data(ptr %64, i64 %65, ptr @3, i64 2, ptr %cat.len3)
  %67 = load i64, ptr %cat.len3, align 4
  %68 = insertvalue { ptr, i64 } undef, ptr %66, 0
  %69 = insertvalue { ptr, i64 } %68, i64 %67, 1
  %70 = call i32 @sere_has_error()
  %71 = icmp ne i32 %70, 0
  br i1 %71, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %if.then
  ret { ptr, i64 } %69

err8:                                             ; preds = %if.end
  ret { ptr, i64 } zeroinitializer

err.ok9:                                          ; preds = %if.end
  ret { ptr, i64 } %35
}

define %File @fs_open({ ptr, i64 } %path, { ptr, i64 } %mode) {
entry:
  %path1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %path, ptr %path1, align 8
  %mode2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %mode, ptr %mode2, align 8
  %init.tmp = alloca %File, align 8
  store %File zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %File, ptr %init.tmp, i32 0, i32 0
  store i32 723007075, ptr %0, align 4
  %1 = load { ptr, i64 }, ptr %path1, align 8
  %2 = load { ptr, i64 }, ptr %mode2, align 8
  call void @fs_File___init__(ptr %init.tmp, { ptr, i64 } %1, { ptr, i64 } %2)
  %3 = load %File, ptr %init.tmp, align 8
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %File zeroinitializer

err.ok:                                           ; preds = %entry
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok
  ret %File zeroinitializer

err.ok4:                                          ; preds = %err.ok
  ret %File %3
}

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

define i32 @sere_main() {
entry:
  %f = alloca %File, align 8
  %found = alloca ptr, align 8
  %text = alloca { ptr, i64 }, align 8
  call void @sere.module.init()
  %ext.str.data = alloca ptr, align 8
  %ext.str.len = alloca i64, align 8
  call void @sere_input(ptr @7, i64 7, ptr %ext.str.data, ptr %ext.str.len)
  %0 = load i64, ptr %ext.str.len, align 4
  %1 = load ptr, ptr %ext.str.data, align 8
  %2 = insertvalue { ptr, i64 } undef, ptr %1, 0
  %3 = insertvalue { ptr, i64 } %2, i64 %0, 1
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  store { ptr, i64 } %3, ptr %text, align 8
  %6 = call ptr @sere_list_new(i64 16)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i32 0

err.ok2:                                          ; preds = %err.ok
  store ptr %6, ptr %found, align 8
  %9 = load { ptr, i64 }, ptr %text, align 8
  %10 = extractvalue { ptr, i64 } %9, 0
  %11 = extractvalue { ptr, i64 } %9, 1
  %12 = call ptr @sere_string_split(ptr %10, i64 %11, ptr @8, i64 0)
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok2
  ret i32 0

err.ok4:                                          ; preds = %err.ok2
  %for.i = alloca i64, align 8
  %c = alloca { ptr, i64 }, align 8
  store i64 0, ptr %for.i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.next, %err.ok4
  %15 = load i64, ptr %for.i, align 4
  %16 = call i64 @sere_list_len(ptr %12)
  %17 = icmp slt i64 %15, %16
  br i1 %17, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %18 = call ptr @sere_list_item(ptr %12, i64 %15)
  %19 = load { ptr, i64 }, ptr %18, align 8
  store { ptr, i64 } %19, ptr %c, align 8
  %20 = load { ptr, i64 }, ptr %c, align 8
  %21 = extractvalue { ptr, i64 } %20, 0
  %22 = extractvalue { ptr, i64 } %20, 1
  %23 = call i32 @sere_str_contains(ptr @9, i64 5, ptr %21, i64 %22)
  %24 = icmp ne i32 %23, 0
  br i1 %24, label %if.then, label %if.next

for.end:                                          ; preds = %for.cond
  %25 = load ptr, ptr %found, align 8
  %26 = call i64 @sere_list_len(ptr %25)
  %27 = call i32 @sere_has_error()
  %28 = icmp ne i32 %27, 0
  br i1 %28, label %err11, label %err.ok12

for.next:                                         ; preds = %err.ok10
  %29 = load i64, ptr %for.i, align 4
  %30 = add i64 %29, 1
  store i64 %30, ptr %for.i, align 4
  br label %for.cond

if.end:                                           ; preds = %if.next, %err.ok8
  %31 = call i32 @sere_has_error()
  %32 = icmp ne i32 %31, 0
  br i1 %32, label %err9, label %err.ok10

if.then:                                          ; preds = %for.body
  %33 = load ptr, ptr %found, align 8
  %34 = load ptr, ptr %found, align 8
  %35 = load { ptr, i64 }, ptr %c, align 8
  %tmp.slot = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %35, ptr %tmp.slot, align 8
  call void @sere_list_push(ptr %34, ptr %tmp.slot)
  %36 = call i32 @sere_has_error()
  %37 = icmp ne i32 %36, 0
  br i1 %37, label %err5, label %err.ok6

if.next:                                          ; preds = %for.body
  br label %if.end

err5:                                             ; preds = %if.then
  ret i32 0

err.ok6:                                          ; preds = %if.then
  %38 = call i32 @sere_has_error()
  %39 = icmp ne i32 %38, 0
  br i1 %39, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret i32 0

err.ok8:                                          ; preds = %err.ok6
  br label %if.end

err9:                                             ; preds = %if.end
  ret i32 0

err.ok10:                                         ; preds = %if.end
  br label %for.next

err11:                                            ; preds = %for.end
  ret i32 0

err.ok12:                                         ; preds = %for.end
  %str.len = alloca i64, align 8
  %40 = call ptr @sere_str_i64_data(i64 %26, ptr %str.len)
  %41 = load i64, ptr %str.len, align 4
  %42 = insertvalue { ptr, i64 } undef, ptr %40, 0
  %43 = insertvalue { ptr, i64 } %42, i64 %41, 1
  %cat.len = alloca i64, align 8
  %44 = extractvalue { ptr, i64 } %43, 0
  %45 = extractvalue { ptr, i64 } %43, 1
  %46 = call ptr @sere_str_concat_data(ptr @11, i64 6, ptr %44, i64 %45, ptr %cat.len)
  %47 = load i64, ptr %cat.len, align 4
  %48 = insertvalue { ptr, i64 } undef, ptr %46, 0
  %49 = insertvalue { ptr, i64 } %48, i64 %47, 1
  %cat.len13 = alloca i64, align 8
  %50 = extractvalue { ptr, i64 } %49, 0
  %51 = extractvalue { ptr, i64 } %49, 1
  %52 = call ptr @sere_str_concat_data(ptr %50, i64 %51, ptr @12, i64 7, ptr %cat.len13)
  %53 = load i64, ptr %cat.len13, align 4
  %54 = insertvalue { ptr, i64 } undef, ptr %52, 0
  %55 = insertvalue { ptr, i64 } %54, i64 %53, 1
  %56 = extractvalue { ptr, i64 } %55, 0
  %57 = extractvalue { ptr, i64 } %55, 1
  call void @sere_write(ptr %56, i64 %57)
  call void @sere_write_nl()
  %58 = call i32 @sere_has_error()
  %59 = icmp ne i32 %58, 0
  br i1 %59, label %err14, label %err.ok15

err14:                                            ; preds = %err.ok12
  ret i32 0

err.ok15:                                         ; preds = %err.ok12
  %60 = load ptr, ptr %found, align 8
  %61 = call { ptr, i64 } @"__sere_repr_list[str]"(ptr %60)
  %62 = extractvalue { ptr, i64 } %61, 0
  %63 = extractvalue { ptr, i64 } %61, 1
  call void @sere_write(ptr %62, i64 %63)
  call void @sere_write_nl()
  %64 = call i32 @sere_has_error()
  %65 = icmp ne i32 %64, 0
  br i1 %65, label %err16, label %err.ok17

err16:                                            ; preds = %err.ok15
  ret i32 0

err.ok17:                                         ; preds = %err.ok15
  %66 = call %File @fs_open({ ptr, i64 } { ptr @18, i64 10 }, { ptr, i64 } { ptr @19, i64 1 })
  %67 = call i32 @sere_has_error()
  %68 = icmp ne i32 %67, 0
  br i1 %68, label %err18, label %err.ok19

err18:                                            ; preds = %err.ok17
  ret i32 0

err.ok19:                                         ; preds = %err.ok17
  %with.self = alloca %File, align 8
  store %File %66, ptr %with.self, align 8
  %69 = call %File @fs_File___enter__(ptr %with.self)
  %70 = call i32 @sere_has_error()
  %71 = icmp ne i32 %70, 0
  br i1 %71, label %err20, label %err.ok21

err20:                                            ; preds = %err.ok19
  ret i32 0

err.ok21:                                         ; preds = %err.ok19
  store %File %69, ptr %f, align 8
  %72 = load ptr, ptr %found, align 8
  %73 = call { ptr, i64 } @"__sere_repr_list[str]"(ptr %72)
  %cat.len22 = alloca i64, align 8
  %74 = extractvalue { ptr, i64 } %73, 0
  %75 = extractvalue { ptr, i64 } %73, 1
  %76 = call ptr @sere_str_concat_data(ptr @20, i64 8, ptr %74, i64 %75, ptr %cat.len22)
  %77 = load i64, ptr %cat.len22, align 4
  %78 = insertvalue { ptr, i64 } undef, ptr %76, 0
  %79 = insertvalue { ptr, i64 } %78, i64 %77, 1
  %80 = call i1 @fs_File_write(ptr %f, { ptr, i64 } %79)
  %81 = call i32 @sere_has_error()
  %82 = icmp ne i32 %81, 0
  br i1 %82, label %err23, label %err.ok24

err23:                                            ; preds = %err.ok21
  ret i32 0

err.ok24:                                         ; preds = %err.ok21
  %83 = call i32 @sere_has_error()
  %84 = icmp ne i32 %83, 0
  br i1 %84, label %err25, label %err.ok26

err25:                                            ; preds = %err.ok24
  ret i32 0

err.ok26:                                         ; preds = %err.ok24
  call void @fs_File___exit__(ptr %with.self)
  %85 = call i32 @sere_has_error()
  %86 = icmp ne i32 %85, 0
  br i1 %86, label %err27, label %err.ok28

err27:                                            ; preds = %err.ok26
  ret i32 0

err.ok28:                                         ; preds = %err.ok26
  %87 = call %File @fs_open({ ptr, i64 } { ptr @21, i64 10 }, { ptr, i64 } { ptr @22, i64 1 })
  %88 = call i32 @sere_has_error()
  %89 = icmp ne i32 %88, 0
  br i1 %89, label %err29, label %err.ok30

err29:                                            ; preds = %err.ok28
  ret i32 0

err.ok30:                                         ; preds = %err.ok28
  %with.self31 = alloca %File, align 8
  store %File %87, ptr %with.self31, align 8
  %90 = call %File @fs_File___enter__(ptr %with.self31)
  %91 = call i32 @sere_has_error()
  %92 = icmp ne i32 %91, 0
  br i1 %92, label %err32, label %err.ok33

err32:                                            ; preds = %err.ok30
  ret i32 0

err.ok33:                                         ; preds = %err.ok30
  store %File %90, ptr %f, align 8
  %93 = call { ptr, i64 } @fs_File_read(ptr %f)
  %94 = call i32 @sere_has_error()
  %95 = icmp ne i32 %94, 0
  br i1 %95, label %err34, label %err.ok35

err34:                                            ; preds = %err.ok33
  ret i32 0

err.ok35:                                         ; preds = %err.ok33
  %96 = extractvalue { ptr, i64 } %93, 0
  %97 = extractvalue { ptr, i64 } %93, 1
  call void @sere_write(ptr %96, i64 %97)
  call void @sere_write_nl()
  %98 = call i32 @sere_has_error()
  %99 = icmp ne i32 %98, 0
  br i1 %99, label %err36, label %err.ok37

err36:                                            ; preds = %err.ok35
  ret i32 0

err.ok37:                                         ; preds = %err.ok35
  %100 = call i32 @sere_has_error()
  %101 = icmp ne i32 %100, 0
  br i1 %101, label %err38, label %err.ok39

err38:                                            ; preds = %err.ok37
  ret i32 0

err.ok39:                                         ; preds = %err.ok37
  call void @fs_File___exit__(ptr %with.self31)
  %102 = call i32 @sere_has_error()
  %103 = icmp ne i32 %102, 0
  br i1 %103, label %err40, label %err.ok41

err40:                                            ; preds = %err.ok39
  ret i32 0

err.ok41:                                         ; preds = %err.ok39
  %data = alloca ptr, align 8
  %104 = call ptr @sere_list_new(i64 1)
  store ptr %104, ptr %data, align 8
  %105 = call %File @fs_open({ ptr, i64 } { ptr @24, i64 10 }, { ptr, i64 } { ptr @25, i64 2 })
  %106 = call i32 @sere_has_error()
  %107 = icmp ne i32 %106, 0
  br i1 %107, label %err42, label %err.ok43

err42:                                            ; preds = %err.ok41
  ret i32 0

err.ok43:                                         ; preds = %err.ok41
  %with.self44 = alloca %File, align 8
  store %File %105, ptr %with.self44, align 8
  %108 = call %File @fs_File___enter__(ptr %with.self44)
  %109 = call i32 @sere_has_error()
  %110 = icmp ne i32 %109, 0
  br i1 %110, label %err45, label %err.ok46

err45:                                            ; preds = %err.ok43
  ret i32 0

err.ok46:                                         ; preds = %err.ok43
  store %File %108, ptr %f, align 8
  %111 = call ptr @fs_File_read_bytes(ptr %f, i64 6)
  %112 = call i32 @sere_has_error()
  %113 = icmp ne i32 %112, 0
  br i1 %113, label %err47, label %err.ok48

err47:                                            ; preds = %err.ok46
  ret i32 0

err.ok48:                                         ; preds = %err.ok46
  store ptr %111, ptr %data, align 8
  %114 = call i32 @sere_has_error()
  %115 = icmp ne i32 %114, 0
  br i1 %115, label %err49, label %err.ok50

err49:                                            ; preds = %err.ok48
  ret i32 0

err.ok50:                                         ; preds = %err.ok48
  %116 = load ptr, ptr %data, align 8
  %117 = call { ptr, i64 } @"__sere_repr_list[i8]"(ptr %116)
  %118 = extractvalue { ptr, i64 } %117, 0
  %119 = extractvalue { ptr, i64 } %117, 1
  call void @sere_write(ptr %118, i64 %119)
  call void @sere_write_nl()
  %120 = call i32 @sere_has_error()
  %121 = icmp ne i32 %120, 0
  br i1 %121, label %err51, label %err.ok52

err51:                                            ; preds = %err.ok50
  ret i32 0

err.ok52:                                         ; preds = %err.ok50
  %122 = call i32 @sere_has_error()
  %123 = icmp ne i32 %122, 0
  br i1 %123, label %err53, label %err.ok54

err53:                                            ; preds = %err.ok52
  ret i32 0

err.ok54:                                         ; preds = %err.ok52
  call void @fs_File___exit__(ptr %with.self44)
  %124 = call i32 @sere_has_error()
  %125 = icmp ne i32 %124, 0
  br i1 %125, label %err55, label %err.ok56

err55:                                            ; preds = %err.ok54
  ret i32 0

err.ok56:                                         ; preds = %err.ok54
  %126 = call %File @fs_open({ ptr, i64 } { ptr @31, i64 9 }, { ptr, i64 } { ptr @32, i64 2 })
  %127 = call i32 @sere_has_error()
  %128 = icmp ne i32 %127, 0
  br i1 %128, label %err57, label %err.ok58

err57:                                            ; preds = %err.ok56
  ret i32 0

err.ok58:                                         ; preds = %err.ok56
  %with.self59 = alloca %File, align 8
  store %File %126, ptr %with.self59, align 8
  %129 = call %File @fs_File___enter__(ptr %with.self59)
  %130 = call i32 @sere_has_error()
  %131 = icmp ne i32 %130, 0
  br i1 %131, label %err60, label %err.ok61

err60:                                            ; preds = %err.ok58
  ret i32 0

err.ok61:                                         ; preds = %err.ok58
  store %File %129, ptr %f, align 8
  %132 = load ptr, ptr %data, align 8
  %133 = call i1 @fs_File_write_bytes(ptr %f, ptr %132)
  %134 = call i32 @sere_has_error()
  %135 = icmp ne i32 %134, 0
  br i1 %135, label %err62, label %err.ok63

err62:                                            ; preds = %err.ok61
  ret i32 0

err.ok63:                                         ; preds = %err.ok61
  %136 = zext i1 %133 to i8
  call void @sere_write_bool(i8 %136)
  call void @sere_write_nl()
  %137 = call i32 @sere_has_error()
  %138 = icmp ne i32 %137, 0
  br i1 %138, label %err64, label %err.ok65

err64:                                            ; preds = %err.ok63
  ret i32 0

err.ok65:                                         ; preds = %err.ok63
  %139 = call i32 @sere_has_error()
  %140 = icmp ne i32 %139, 0
  br i1 %140, label %err66, label %err.ok67

err66:                                            ; preds = %err.ok65
  ret i32 0

err.ok67:                                         ; preds = %err.ok65
  call void @fs_File___exit__(ptr %with.self59)
  %141 = call i32 @sere_has_error()
  %142 = icmp ne i32 %141, 0
  br i1 %142, label %err68, label %err.ok69

err68:                                            ; preds = %err.ok67
  ret i32 0

err.ok69:                                         ; preds = %err.ok67
  %143 = call %File @fs_open({ ptr, i64 } { ptr @34, i64 9 }, { ptr, i64 } { ptr @35, i64 2 })
  %144 = call i32 @sere_has_error()
  %145 = icmp ne i32 %144, 0
  br i1 %145, label %err70, label %err.ok71

err70:                                            ; preds = %err.ok69
  ret i32 0

err.ok71:                                         ; preds = %err.ok69
  %with.self72 = alloca %File, align 8
  store %File %143, ptr %with.self72, align 8
  %146 = call %File @fs_File___enter__(ptr %with.self72)
  %147 = call i32 @sere_has_error()
  %148 = icmp ne i32 %147, 0
  br i1 %148, label %err73, label %err.ok74

err73:                                            ; preds = %err.ok71
  ret i32 0

err.ok74:                                         ; preds = %err.ok71
  store %File %146, ptr %f, align 8
  %149 = call ptr @fs_File_read_bytes(ptr %f, i64 4096)
  %150 = call i32 @sere_has_error()
  %151 = icmp ne i32 %150, 0
  br i1 %151, label %err75, label %err.ok76

err75:                                            ; preds = %err.ok74
  ret i32 0

err.ok76:                                         ; preds = %err.ok74
  %152 = call { ptr, i64 } @"__sere_repr_list[i8]"(ptr %149)
  %153 = extractvalue { ptr, i64 } %152, 0
  %154 = extractvalue { ptr, i64 } %152, 1
  call void @sere_write(ptr %153, i64 %154)
  call void @sere_write_nl()
  %155 = call i32 @sere_has_error()
  %156 = icmp ne i32 %155, 0
  br i1 %156, label %err77, label %err.ok78

err77:                                            ; preds = %err.ok76
  ret i32 0

err.ok78:                                         ; preds = %err.ok76
  %157 = call i32 @sere_has_error()
  %158 = icmp ne i32 %157, 0
  br i1 %158, label %err79, label %err.ok80

err79:                                            ; preds = %err.ok78
  ret i32 0

err.ok80:                                         ; preds = %err.ok78
  call void @fs_File___exit__(ptr %with.self72)
  %159 = call i32 @sere_has_error()
  %160 = icmp ne i32 %159, 0
  br i1 %160, label %err81, label %err.ok82

err81:                                            ; preds = %err.ok80
  ret i32 0

err.ok82:                                         ; preds = %err.ok80
  %161 = call ptr @sere_list_new(i64 4)
  %range.i = alloca i32, align 4
  store i32 5, ptr %range.i, align 4
  br label %range.cond

range.cond:                                       ; preds = %range.body, %err.ok82
  %162 = load i32, ptr %range.i, align 4
  %163 = icmp slt i32 %162, 0
  %164 = icmp sgt i32 %162, 0
  %165 = select i1 false, i1 %163, i1 %164
  br i1 %165, label %range.body, label %range.end

range.body:                                       ; preds = %range.cond
  %range.el = alloca i32, align 4
  store i32 %162, ptr %range.el, align 4
  call void @sere_list_push(ptr %161, ptr %range.el)
  %166 = add i32 %162, -2
  store i32 %166, ptr %range.i, align 4
  br label %range.cond

range.end:                                        ; preds = %range.cond
  %167 = call i32 @sere_has_error()
  %168 = icmp ne i32 %167, 0
  br i1 %168, label %err83, label %err.ok84

err83:                                            ; preds = %range.end
  ret i32 0

err.ok84:                                         ; preds = %range.end
  %169 = call { ptr, i64 } @"__sere_repr_list[i32]"(ptr %161)
  %170 = extractvalue { ptr, i64 } %169, 0
  %171 = extractvalue { ptr, i64 } %169, 1
  call void @sere_write(ptr %170, i64 %171)
  call void @sere_write_nl()
  %172 = call i32 @sere_has_error()
  %173 = icmp ne i32 %172, 0
  br i1 %173, label %err85, label %err.ok86

err85:                                            ; preds = %err.ok84
  ret i32 0

err.ok86:                                         ; preds = %err.ok84
  call void @sere_write_bool(i8 1)
  call void @sere_write_nl()
  %174 = call i32 @sere_has_error()
  %175 = icmp ne i32 %174, 0
  br i1 %175, label %err87, label %err.ok88

err87:                                            ; preds = %err.ok86
  ret i32 0

err.ok88:                                         ; preds = %err.ok86
  call void @sere_write_bool(i8 1)
  call void @sere_write_nl()
  %176 = call i32 @sere_has_error()
  %177 = icmp ne i32 %176, 0
  br i1 %177, label %err89, label %err.ok90

err89:                                            ; preds = %err.ok88
  ret i32 0

err.ok90:                                         ; preds = %err.ok88
  %178 = call i32 @sere_has_error()
  %179 = icmp ne i32 %178, 0
  br i1 %179, label %err91, label %err.ok92

err91:                                            ; preds = %err.ok90
  ret i32 0

err.ok92:                                         ; preds = %err.ok90
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

declare i32 @sere_has_error()

declare void @sere_free(ptr)

declare void @sere_shared_release(ptr)

declare ptr @sere_list_new(i64)

declare ptr @sere_array_new(i64, i64)

declare void @sere_list_push(ptr, ptr)

declare ptr @sere_list_item(ptr, i64)

declare ptr @sere_alloc(i64)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare ptr @sere_str_concat_data(ptr, i64, ptr, i64, ptr)

declare ptr @sere_string_split(ptr, i64, ptr, i64)

declare i32 @sere_str_contains(ptr, i64, ptr, i64)

declare void @sere_write_nl()

declare ptr @sere_str_i64_data(i64, ptr)

declare void @sere_write(ptr, i64)

define internal { ptr, i64 } @"__sere_repr_list[str]"(ptr %0) {
entry:
  %1 = call i64 @sere_list_len(ptr %0)
  %2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } { ptr @14, i64 1 }, ptr %2, align 8
  br label %loop

loop:                                             ; preds = %item, %entry
  %3 = phi i64 [ 0, %entry ], [ %33, %item ]
  %4 = icmp slt i64 %3, %1
  br i1 %4, label %item, label %end

item:                                             ; preds = %loop
  %5 = icmp eq i64 %3, 0
  %6 = select i1 %5, { ptr, i64 } { ptr @16, i64 0 }, { ptr, i64 } { ptr @15, i64 2 }
  %7 = load { ptr, i64 }, ptr %2, align 8
  %cat.len = alloca i64, align 8
  %8 = extractvalue { ptr, i64 } %7, 0
  %9 = extractvalue { ptr, i64 } %7, 1
  %10 = extractvalue { ptr, i64 } %6, 0
  %11 = extractvalue { ptr, i64 } %6, 1
  %12 = call ptr @sere_str_concat_data(ptr %8, i64 %9, ptr %10, i64 %11, ptr %cat.len)
  %13 = load i64, ptr %cat.len, align 4
  %14 = insertvalue { ptr, i64 } undef, ptr %12, 0
  %15 = insertvalue { ptr, i64 } %14, i64 %13, 1
  %16 = call ptr @sere_list_item(ptr %0, i64 %3)
  %17 = load { ptr, i64 }, ptr %16, align 8
  %18 = alloca i64, align 8
  %19 = extractvalue { ptr, i64 } %17, 0
  %20 = extractvalue { ptr, i64 } %17, 1
  %21 = call ptr @sere_str_repr_data(ptr %19, i64 %20, ptr %18)
  %22 = insertvalue { ptr, i64 } undef, ptr %21, 0
  %23 = load i64, ptr %18, align 4
  %24 = insertvalue { ptr, i64 } %22, i64 %23, 1
  %cat.len1 = alloca i64, align 8
  %25 = extractvalue { ptr, i64 } %15, 0
  %26 = extractvalue { ptr, i64 } %15, 1
  %27 = extractvalue { ptr, i64 } %24, 0
  %28 = extractvalue { ptr, i64 } %24, 1
  %29 = call ptr @sere_str_concat_data(ptr %25, i64 %26, ptr %27, i64 %28, ptr %cat.len1)
  %30 = load i64, ptr %cat.len1, align 4
  %31 = insertvalue { ptr, i64 } undef, ptr %29, 0
  %32 = insertvalue { ptr, i64 } %31, i64 %30, 1
  store { ptr, i64 } %32, ptr %2, align 8
  %33 = add i64 %3, 1
  br label %loop

end:                                              ; preds = %loop
  %34 = load { ptr, i64 }, ptr %2, align 8
  %cat.len2 = alloca i64, align 8
  %35 = extractvalue { ptr, i64 } %34, 0
  %36 = extractvalue { ptr, i64 } %34, 1
  %37 = call ptr @sere_str_concat_data(ptr %35, i64 %36, ptr @17, i64 1, ptr %cat.len2)
  %38 = load i64, ptr %cat.len2, align 4
  %39 = insertvalue { ptr, i64 } undef, ptr %37, 0
  %40 = insertvalue { ptr, i64 } %39, i64 %38, 1
  ret { ptr, i64 } %40
}

declare ptr @sere_str_repr_data(ptr, i64, ptr)

define internal { ptr, i64 } @"__sere_repr_list[i8]"(ptr %0) {
entry:
  %1 = call i64 @sere_list_len(ptr %0)
  %2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } { ptr @27, i64 1 }, ptr %2, align 8
  br label %loop

loop:                                             ; preds = %item, %entry
  %3 = phi i64 [ 0, %entry ], [ %31, %item ]
  %4 = icmp slt i64 %3, %1
  br i1 %4, label %item, label %end

item:                                             ; preds = %loop
  %5 = icmp eq i64 %3, 0
  %6 = select i1 %5, { ptr, i64 } { ptr @29, i64 0 }, { ptr, i64 } { ptr @28, i64 2 }
  %7 = load { ptr, i64 }, ptr %2, align 8
  %cat.len = alloca i64, align 8
  %8 = extractvalue { ptr, i64 } %7, 0
  %9 = extractvalue { ptr, i64 } %7, 1
  %10 = extractvalue { ptr, i64 } %6, 0
  %11 = extractvalue { ptr, i64 } %6, 1
  %12 = call ptr @sere_str_concat_data(ptr %8, i64 %9, ptr %10, i64 %11, ptr %cat.len)
  %13 = load i64, ptr %cat.len, align 4
  %14 = insertvalue { ptr, i64 } undef, ptr %12, 0
  %15 = insertvalue { ptr, i64 } %14, i64 %13, 1
  %16 = call ptr @sere_list_item(ptr %0, i64 %3)
  %17 = load i8, ptr %16, align 1
  %18 = sext i8 %17 to i32
  %str.len = alloca i64, align 8
  %19 = call ptr @sere_str_i32_data(i32 %18, ptr %str.len)
  %20 = load i64, ptr %str.len, align 4
  %21 = insertvalue { ptr, i64 } undef, ptr %19, 0
  %22 = insertvalue { ptr, i64 } %21, i64 %20, 1
  %cat.len1 = alloca i64, align 8
  %23 = extractvalue { ptr, i64 } %15, 0
  %24 = extractvalue { ptr, i64 } %15, 1
  %25 = extractvalue { ptr, i64 } %22, 0
  %26 = extractvalue { ptr, i64 } %22, 1
  %27 = call ptr @sere_str_concat_data(ptr %23, i64 %24, ptr %25, i64 %26, ptr %cat.len1)
  %28 = load i64, ptr %cat.len1, align 4
  %29 = insertvalue { ptr, i64 } undef, ptr %27, 0
  %30 = insertvalue { ptr, i64 } %29, i64 %28, 1
  store { ptr, i64 } %30, ptr %2, align 8
  %31 = add i64 %3, 1
  br label %loop

end:                                              ; preds = %loop
  %32 = load { ptr, i64 }, ptr %2, align 8
  %cat.len2 = alloca i64, align 8
  %33 = extractvalue { ptr, i64 } %32, 0
  %34 = extractvalue { ptr, i64 } %32, 1
  %35 = call ptr @sere_str_concat_data(ptr %33, i64 %34, ptr @30, i64 1, ptr %cat.len2)
  %36 = load i64, ptr %cat.len2, align 4
  %37 = insertvalue { ptr, i64 } undef, ptr %35, 0
  %38 = insertvalue { ptr, i64 } %37, i64 %36, 1
  ret { ptr, i64 } %38
}

declare ptr @sere_str_i32_data(i32, ptr)

declare void @sere_write_bool(i8)

define internal { ptr, i64 } @"__sere_repr_list[i32]"(ptr %0) {
entry:
  %1 = call i64 @sere_list_len(ptr %0)
  %2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } { ptr @38, i64 1 }, ptr %2, align 8
  br label %loop

loop:                                             ; preds = %item, %entry
  %3 = phi i64 [ 0, %entry ], [ %30, %item ]
  %4 = icmp slt i64 %3, %1
  br i1 %4, label %item, label %end

item:                                             ; preds = %loop
  %5 = icmp eq i64 %3, 0
  %6 = select i1 %5, { ptr, i64 } { ptr @40, i64 0 }, { ptr, i64 } { ptr @39, i64 2 }
  %7 = load { ptr, i64 }, ptr %2, align 8
  %cat.len = alloca i64, align 8
  %8 = extractvalue { ptr, i64 } %7, 0
  %9 = extractvalue { ptr, i64 } %7, 1
  %10 = extractvalue { ptr, i64 } %6, 0
  %11 = extractvalue { ptr, i64 } %6, 1
  %12 = call ptr @sere_str_concat_data(ptr %8, i64 %9, ptr %10, i64 %11, ptr %cat.len)
  %13 = load i64, ptr %cat.len, align 4
  %14 = insertvalue { ptr, i64 } undef, ptr %12, 0
  %15 = insertvalue { ptr, i64 } %14, i64 %13, 1
  %16 = call ptr @sere_list_item(ptr %0, i64 %3)
  %17 = load i32, ptr %16, align 4
  %str.len = alloca i64, align 8
  %18 = call ptr @sere_str_i32_data(i32 %17, ptr %str.len)
  %19 = load i64, ptr %str.len, align 4
  %20 = insertvalue { ptr, i64 } undef, ptr %18, 0
  %21 = insertvalue { ptr, i64 } %20, i64 %19, 1
  %cat.len1 = alloca i64, align 8
  %22 = extractvalue { ptr, i64 } %15, 0
  %23 = extractvalue { ptr, i64 } %15, 1
  %24 = extractvalue { ptr, i64 } %21, 0
  %25 = extractvalue { ptr, i64 } %21, 1
  %26 = call ptr @sere_str_concat_data(ptr %22, i64 %23, ptr %24, i64 %25, ptr %cat.len1)
  %27 = load i64, ptr %cat.len1, align 4
  %28 = insertvalue { ptr, i64 } undef, ptr %26, 0
  %29 = insertvalue { ptr, i64 } %28, i64 %27, 1
  store { ptr, i64 } %29, ptr %2, align 8
  %30 = add i64 %3, 1
  br label %loop

end:                                              ; preds = %loop
  %31 = load { ptr, i64 }, ptr %2, align 8
  %cat.len2 = alloca i64, align 8
  %32 = extractvalue { ptr, i64 } %31, 0
  %33 = extractvalue { ptr, i64 } %31, 1
  %34 = call ptr @sere_str_concat_data(ptr %32, i64 %33, ptr @41, i64 1, ptr %cat.len2)
  %35 = load i64, ptr %cat.len2, align 4
  %36 = insertvalue { ptr, i64 } undef, ptr %34, 0
  %37 = insertvalue { ptr, i64 } %36, i64 %35, 1
  ret { ptr, i64 } %37
}

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
