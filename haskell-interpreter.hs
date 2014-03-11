-- brainfuck interpreter in haskell
-- non-compiling ver. for minimal memory use
-- assuming compiled*tag size is bigger than the original source anyway
-- http://astrange.ithinksw.net/tools/bfi/programs/99bottles.bf

module Main (main) where

import Data.Array.IO -- cthulhu fhtagn
import Data.ByteString (ByteString)
import qualified Data.ByteString as B
import Data.Word
import Data.Map (Map)
import qualified Data.Map as Map
import System.Environment
import System.IO

type Jumps = Map Int Int
type Bytes = IOUArray Int Word8

-- record syntax eating the namespace is awful
-- just using tuples is easier, hopefully not slower
-- names:
-- prog@(text,jumps)
-- state@(cells,ptr,pc)
type Prog = (ByteString,Jumps)
type State = (Bytes,Int,Int)

chr8 :: Word8 -> Char
chr8 = toEnum . fromEnum
ord8 :: Char -> Word8
ord8 = toEnum . fromEnum

-- optimizations:
-- reorder to use : instead of ++ (does this matter for lazy lists?)
-- try to output a sorted list, fromAscList is much better than fromList
-- only do one program text walk, instead of one to find jumps and at least one in execute
compile' :: ByteString -> Int -> [(Int,Int)] -> [Int] -> [(Int,Int)]
compile' bf pos jumps stack = let
	ch = chr8 $ bf `B.index` pos
	bflen = B.length bf
	stackPush = pos:stack
	lastL:stackPop = stack
	moreJumps = jumps ++ [(lastL,pos+1),(pos,lastL+1)]
	pos1 = pos+1
	next = case ch of
		'[' -> compile' bf pos1 jumps stackPush
		']' -> compile' bf pos1 moreJumps stackPop
		otherwise -> compile' bf pos1 jumps stack
	in
	if (pos < bflen) then next else jumps

compile :: ByteString -> Prog
compile bf = (bf,Map.fromList $ compile' bf 0 [] [])

execute :: Prog -> State -> IO ()
execute prog@(text,jumps) state@(cells,ptr,pc) = let
	insn = chr8 $ text `B.index` pc
	insns = B.length text
	pc1 = pc + 1
	pcJ = jumps Map.! pc
	next = execute prog (cells,ptr,pc1)
	cellIncDec n = do
		cell <- readArray cells ptr
		writeArray cells ptr (cell + n)
	putc = do
		cell <- readArray cells ptr
		putChar $ chr8 cell
	setc c = do
		writeArray cells ptr (ord8 c)
	ioAct a = do
		a
		next
	jump cond = do
		cell <- readArray cells ptr
		let pc' = if (cond cell) then pcJ else pc1
		execute prog (cells,ptr,pc')
	ptrIncDec n = execute prog (cells,ptr + n,pc1)
	exec = case insn of
			'>' -> ptrIncDec 1
			'<' -> ptrIncDec (-1) 
			'+' -> ioAct $ cellIncDec 1
			'-' -> ioAct $ cellIncDec (-1)
			'.' -> ioAct $ putc
			',' -> ioAct $ getChar >>= setc
			'[' -> jump (\c -> c == 0)
			']' -> jump (\c -> c /= 0)
			otherwise -> next
	in
	if (pc < insns) then exec else return ()
	
runBF bf = do
		startCells <- newArray (0, 65535) 0
		let state = (startCells,0,0)
		let prog = compile bf
		execute prog state

main = do
	args <- getArgs
	bf <- B.readFile $ head args
	hSetBuffering stdout NoBuffering
	runBF bf