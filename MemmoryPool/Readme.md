
- Singe CPP header for memory pool
- Sample usage:
``` 
unsigned long ulUnitNum = 4;
unsigned long ulUnitSize = 32;
MemPool* pool = new MemPool(ulUnitNum,ulUnitSize);
void* p1 = pool->Alloc(ulUnitSize);
pool->Free(p1);
delete pool; 
```
