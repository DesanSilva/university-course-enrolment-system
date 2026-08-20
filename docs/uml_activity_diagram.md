# NexusEnroll — Activity Diagram (Student Enrolment)

```mermaid
stateDiagram-v2
    [*] --> StartTransaction
    StartTransaction --> CheckStudentActive
    
    CheckStudentActive --> FetchOffering : Valid & Active
    CheckStudentActive --> [*] : Invalid/Inactive (Error)
    
    FetchOffering --> CheckEnrollmentExists : Offering Found
    FetchOffering --> [*] : Offering Not Found (Error)
    
    CheckEnrollmentExists --> CheckPrerequisites : Not Enrolled
    CheckEnrollmentExists --> [*] : Already Enrolled (Error)
    
    CheckPrerequisites --> CheckCapacity : Prerequisites Met
    CheckPrerequisites --> [*] : Missing Prerequisites (Error)
    
    CheckCapacity --> CheckTimeConflict : Seats Available
    CheckCapacity --> [*] : Full (Error)
    
    CheckTimeConflict --> CheckWaitlistEntry : No Conflict
    CheckTimeConflict --> [*] : Time Conflict (Error)
    
    CheckWaitlistEntry --> SaveEnrollmentRecord
    SaveEnrollmentRecord --> RetireWaitlistEntryIfPresent
    RetireWaitlistEntryIfPresent --> CommitTransaction
    
    CommitTransaction --> Success
    Success --> [*]
```
