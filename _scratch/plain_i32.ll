; ModuleID = '.\_scratch\plain_i32.sere'
source_filename = ".\\_scratch\\plain_i32.sere"

%Exception = type { ptr }

define i32 @abs(i32 %0) {
entry:
  %1 = icmp slt i32 %0, 0
  br i1 %1, label %if.body0, label %if.end

if.end:                                           ; preds = %entry
  ret i32 %0

if.body0:                                         ; preds = %entry
  %2 = sub i32 0, %0
  ret i32 %2
}

define i32 @min(i32 %0, i32 %1) {
entry:
  %2 = icmp slt i32 %0, %1
  br i1 %2, label %if.body0, label %if.body1

if.end:                                           ; No predecessors!
  unreachable

if.body0:                                         ; preds = %entry
  ret i32 %0

if.body1:                                         ; preds = %entry
  ret i32 %1
}

define i32 @max(i32 %0, i32 %1) {
entry:
  %2 = icmp sgt i32 %0, %1
  br i1 %2, label %if.body0, label %if.body1

if.end:                                           ; No predecessors!
  unreachable

if.body0:                                         ; preds = %entry
  ret i32 %0

if.body1:                                         ; preds = %entry
  ret i32 %1
}

define i32 @clamp(i32 %0, i32 %1, i32 %2) {
entry:
  %3 = call i32 @max(i32 %0, i32 %1)
  %4 = call i32 @min(i32 %3, i32 %2)
  ret i32 %4
}

define i32 @sign(i32 %0) {
entry:
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %if.body0, label %if.body1

if.end:                                           ; preds = %if.body1
  ret i32 0

if.body0:                                         ; preds = %entry
  ret i32 1

if.body1:                                         ; preds = %if.body1, %entry
  %2 = icmp slt i32 %0, 0
  br i1 %2, label %if.body1, label %if.end
}

define void @prelude___init__(%Exception %0, ptr %1) {
entry:
  ret void
}

define i32 @sere_main() {
entry:
  ret i32 0
}
